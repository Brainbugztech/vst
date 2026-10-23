#pragma once

// Brainfuzz - analoge Filter-Prototypen und digitale Biquad-Kaskaden
// Typen: Butterworth, Linkwitz-Riley, Chebyshev I/II, Elliptic, Bessel, Legendre, Gaussian
// Alle Tiefpass-Prototypen (ausser LR) sind auf -3 dB bei w = 1 normiert, LR auf -6 dB.
// Ohne JUCE, damit die Engine auch in den Test-Tools laeuft.

#include <array>
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace bfz
{
using cd = std::complex<double>;
static constexpr double kPi = 3.14159265358979323846;

enum class XoverType { butterworth = 0, linkwitzRiley, chebyshev1, chebyshev2, elliptic, bessel, legendre, gaussian, count };

inline const char* xoverTypeName (int t)
{
    static const char* names[] { "Butterworth", "Linkwitz-Riley", "Chebyshev I", "Chebyshev II",
                                 "Elliptic", "Bessel", "Legendre", "Gaussian" };
    return names[std::clamp (t, 0, 7)];
}

// Analoger Prototyp: Pole, Nullstellen, Verstaerkung (Tiefpass, Grenzfrequenz 1 rad/s)
struct Zpk
{
    std::vector<cd> poles, zeros;
    double gain = 1.0;

    cd eval (cd s) const
    {
        cd num (gain, 0.0), den (1.0, 0.0);
        for (auto z : zeros) num *= (s - z);
        for (auto p : poles) den *= (s - p);
        return num / den;
    }
    double mag (double w) const { return std::abs (eval (cd (0.0, w))); }
};

// ------------------------------------------------------------------ Polynom-Hilfen
// Polynom als Koeffizienten aufsteigend: c[0] + c[1] x + ...
using Poly = std::vector<double>;

inline std::vector<cd> polyRoots (const Poly& cIn)
{
    Poly c = cIn;
    while (c.size() > 1 && std::abs (c.back()) < 1e-300) c.pop_back();
    const int n = (int) c.size() - 1;
    std::vector<cd> r ((size_t) n);
    if (n < 1) return r;
    std::vector<cd> a ((size_t) n + 1);
    for (int i = 0; i <= n; ++i) a[(size_t) i] = c[(size_t) i] / c[(size_t) n];

    auto evalP = [&] (cd x)
    {
        cd v = a[(size_t) n];
        for (int i = n - 1; i >= 0; --i) v = v * x + a[(size_t) i];
        return v;
    };
    auto evalD = [&] (cd x)
    {
        cd v = a[(size_t) n] * (double) n;
        for (int i = n - 1; i >= 1; --i) v = v * x + a[(size_t) i] * (double) i;
        return v;
    };

    // Startwerte auf einem Kreis (Radius aus Koeffizienten)
    double rad = 0.0;
    for (int i = 0; i < n; ++i) rad = std::max (rad, std::pow (std::abs (a[(size_t) i]), 1.0 / (n - i)));
    rad = std::max (rad, 0.5);
    for (int i = 0; i < n; ++i)
        r[(size_t) i] = std::polar (rad, 2.0 * kPi * (i + 0.25) / n + 0.4);

    // Aberth-Ehrlich
    for (int it = 0; it < 500; ++it)
    {
        double maxStep = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const cd p = evalP (r[(size_t) i]), d = evalD (r[(size_t) i]);
            if (std::abs (d) < 1e-300) continue;
            const cd ratio = p / d;
            cd sum (0.0, 0.0);
            for (int j = 0; j < n; ++j)
                if (j != i) sum += 1.0 / (r[(size_t) i] - r[(size_t) j]);
            const cd step = ratio / (1.0 - ratio * sum);
            r[(size_t) i] -= step;
            maxStep = std::max (maxStep, std::abs (step) / std::max (1.0, std::abs (r[(size_t) i])));
        }
        if (maxStep < 1e-15) break;
    }
    // Newton-Politur
    for (auto& x : r)
        for (int it = 0; it < 4; ++it)
        {
            const cd d = evalD (x);
            if (std::abs (d) > 1e-300) x -= evalP (x) / d;
        }
    return r;
}

inline Poly polyMul (const Poly& a, const Poly& b)
{
    Poly r (a.size() + b.size() - 1, 0.0);
    for (size_t i = 0; i < a.size(); ++i)
        for (size_t j = 0; j < b.size(); ++j) r[i + j] += a[i] * b[j];
    return r;
}

// Aus |H(jw)|^2 = 1 / D(w^2) die Pole (linke Halbebene) bestimmen: D(-s^2) = 0
inline std::vector<cd> polesFromMagSquaredDen (const Poly& dOfU)
{
    // D(u) mit u = w^2 = -s^2  ->  Polynom in s
    Poly ps (dOfU.size() * 2 - 1, 0.0);
    for (size_t k = 0; k < dOfU.size(); ++k)
        ps[2 * k] = dOfU[k] * ((k % 2) ? -1.0 : 1.0);
    auto roots = polyRoots (ps);
    std::vector<cd> lhp;
    for (auto r : roots) if (r.real() < 0.0) lhp.push_back (r);
    std::sort (lhp.begin(), lhp.end(), [] (cd a, cd b) { return a.imag() < b.imag(); });
    return lhp;
}

// ------------------------------------------------------------------ Normierung
inline void setDcGain (Zpk& z, double target = 1.0)
{
    z.gain = 1.0;
    // Bei Nullstellen auf der j-Achse ist DC nicht 0, nur Pole/Zeros werten
    const double m = z.mag (0.0);
    if (m > 0.0) z.gain = target / m;
}

// Frequenz suchen, bei der |H| = level, und alle Pole/Nullstellen so skalieren, dass sie bei w = 1 liegt
inline void normaliseCutoff (Zpk& z, double level)
{
    setDcGain (z);
    double lo = 1e-3, hi = 1e3;
    // |H| faellt (fast) monoton um die Grenzfrequenz - Bisektion auf log-Skala, von unten gesucht
    // Erst grob: erstes w, bei dem |H| < level
    double w = lo;
    const double stepF = 1.02;
    while (w < hi && z.mag (w) >= level) w *= stepF;
    lo = w / stepF; hi = w;
    for (int i = 0; i < 80; ++i)
    {
        const double mid = std::sqrt (lo * hi);
        if (z.mag (mid) >= level) lo = mid; else hi = mid;
    }
    const double wc = std::sqrt (lo * hi);
    for (auto& p : z.poles) p /= wc;
    for (auto& q : z.zeros) q /= wc;
    setDcGain (z);
}

// ------------------------------------------------------------------ Elliptische Funktionen (nach Orfanidis)
inline std::vector<double> landen (double k, int m = 8)
{
    std::vector<double> v;
    for (int i = 0; i < m; ++i)
    {
        k = k / (1.0 + std::sqrt (1.0 - k * k));
        k = k * k;
        v.push_back (k);
    }
    return v;
}

inline cd cde (cd u, double k)
{
    auto v = landen (k);
    cd w = std::cos (u * kPi / 2.0);
    for (int i = (int) v.size() - 1; i >= 0; --i)
        w = (1.0 + v[(size_t) i]) * w / (1.0 + v[(size_t) i] * w * w);
    return w;
}

inline cd sne (cd u, double k)
{
    auto v = landen (k);
    cd w = std::sin (u * kPi / 2.0);
    for (int i = (int) v.size() - 1; i >= 0; --i)
        w = (1.0 + v[(size_t) i]) * w / (1.0 + v[(size_t) i] * w * w);
    return w;
}

inline cd acde (cd w, double k)
{
    auto v = landen (k);
    double kPrev = k;
    for (size_t i = 0; i < v.size(); ++i)
    {
        w = w / (1.0 + std::sqrt (1.0 - w * w * kPrev * kPrev)) * 2.0 / (1.0 + v[i]);
        kPrev = v[i];
    }
    return 2.0 / kPi * std::acos (w);
}

inline cd asne (cd w, double k) { return 1.0 - acde (w, k); }

inline double ellipdeg (int n, double k1)
{
    const int L = n / 2;
    const double kc1 = std::sqrt (1.0 - k1 * k1);
    double prod = 1.0;
    for (int i = 1; i <= L; ++i)
    {
        const double ui = (2.0 * i - 1.0) / n;
        prod *= std::pow (sne (cd (ui, 0.0), kc1).real(), 4.0);
    }
    const double kc = std::pow (kc1, (double) n) * prod;
    return std::sqrt (1.0 - kc * kc);
}

// ------------------------------------------------------------------ Prototypen
inline Zpk butterworth (int n)
{
    Zpk z;
    for (int k = 0; k < n; ++k)
    {
        const double th = kPi * (2.0 * k + 1.0 + n) / (2.0 * n);
        z.poles.push_back (std::polar (1.0, th));
    }
    setDcGain (z);
    return z;
}

inline Zpk chebyshev1 (int n, double rippleDb = 1.0)
{
    Zpk z;
    const double eps = std::sqrt (std::pow (10.0, rippleDb / 10.0) - 1.0);
    const double mu = std::asinh (1.0 / eps) / n;
    for (int k = 0; k < n; ++k)
    {
        const double th = kPi * (2.0 * k + 1.0) / (2.0 * n);
        z.poles.push_back (cd (-std::sinh (mu) * std::sin (th), std::cosh (mu) * std::cos (th)));
    }
    normaliseCutoff (z, std::sqrt (0.5));
    return z;
}

inline Zpk chebyshev2 (int n, double stopDb = 40.0)
{
    Zpk z;
    const double eps = 1.0 / std::sqrt (std::pow (10.0, stopDb / 10.0) - 1.0);
    const double mu = std::asinh (1.0 / eps) / n;
    for (int k = 0; k < n; ++k)
    {
        const double th = kPi * (2.0 * k + 1.0) / (2.0 * n);
        const cd p (-std::sinh (mu) * std::sin (th), std::cosh (mu) * std::cos (th));
        z.poles.push_back (1.0 / p);
        const double c = std::cos (th);
        if (std::abs (c) > 1e-12) z.zeros.push_back (cd (0.0, 1.0 / c));
    }
    normaliseCutoff (z, std::sqrt (0.5));
    return z;
}

inline Zpk elliptic (int n, double rippleDb = 1.0, double stopDb = 45.0)
{
    Zpk z;
    if (n < 2) return butterworth (n);
    const double ep = std::sqrt (std::pow (10.0, rippleDb / 10.0) - 1.0);
    const double es = std::sqrt (std::pow (10.0, stopDb / 10.0) - 1.0);
    const double k1 = ep / es;
    const double k = ellipdeg (n, k1);
    const int L = n / 2;
    const cd j (0.0, 1.0);
    const cd v0 = -j * asne (j / ep, k1) / (double) n;
    for (int i = 1; i <= L; ++i)
    {
        const double ui = (2.0 * i - 1.0) / n;
        const cd zeta = cde (cd (ui, 0.0), k);
        const cd zr = j / (k * zeta);
        z.zeros.push_back (zr);
        z.zeros.push_back (std::conj (zr));
        const cd p = j * cde (ui - j * v0, k);
        z.poles.push_back (p);
        z.poles.push_back (std::conj (p));
    }
    if (n % 2) z.poles.push_back (j * sne (j * v0, k));
    for (auto& p : z.poles) if (p.real() > 0.0) p = -std::conj (p); // Sicherheit
    normaliseCutoff (z, std::sqrt (0.5));
    return z;
}

inline Zpk bessel (int n)
{
    // Bessel-Polynom: a_k = (2n-k)! / (2^(n-k) k! (n-k)!)
    Poly c ((size_t) n + 1);
    auto fact = [] (int x) { double f = 1.0; for (int i = 2; i <= x; ++i) f *= i; return f; };
    for (int k = 0; k <= n; ++k)
        c[(size_t) k] = fact (2 * n - k) / (std::pow (2.0, n - k) * fact (k) * fact (n - k));
    Zpk z;
    for (auto r : polyRoots (c)) z.poles.push_back (r.real() < 0.0 ? r : -std::conj (r));
    normaliseCutoff (z, std::sqrt (0.5));
    return z;
}

inline Zpk gaussian (int n)
{
    // |H|^2 = 1 / sum_{k=0..n} (w^2)^k / k!   (abgebrochene Gauss-Reihe)
    Poly d ((size_t) n + 1);
    double f = 1.0;
    for (int k = 0; k <= n; ++k) { if (k > 0) f *= k; d[(size_t) k] = 1.0 / f; }
    Zpk z;
    z.poles = polesFromMagSquaredDen (d);
    normaliseCutoff (z, std::sqrt (0.5));
    return z;
}

// Legendre ("optimum L", Papoulis/Fukada): |H|^2 = 1 / (1 + L_n(w^2))
// L_n(u) = Integral_{-1}^{2u-1} wgt(x) v(x)^2 dx, v = Kern der orthonormalen Polynome zur Gewichtsfunktion
// ungerade n: Gewicht 1, Grad k = (n-1)/2; gerade n: Gewicht (1+x), Grad k = (n-2)/2
inline Poly legendreL (int n)
{
    const bool even = (n % 2) == 0;
    const int deg = even ? (n - 2) / 2 : (n - 1) / 2;
    // Momente: m_j = Integral x^j w(x) dx auf [-1, 1]
    auto moment = [&] (int jx)
    {
        auto mx = [] (int e) { return (e % 2) ? 0.0 : 2.0 / (e + 1); };
        return even ? mx (jx) + mx (jx + 1) : mx (jx);
    };
    auto inner = [&] (const Poly& a, const Poly& b)
    {
        double s = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            for (size_t k = 0; k < b.size(); ++k) s += a[i] * b[k] * moment ((int) (i + k));
        return s;
    };
    // Gram-Schmidt
    std::vector<Poly> ortho;
    for (int d = 0; d <= deg; ++d)
    {
        Poly p ((size_t) d + 1, 0.0); p[(size_t) d] = 1.0;
        for (const auto& q : ortho)
        {
            const double c = inner (p, q);
            for (size_t i = 0; i < q.size(); ++i) p[i] -= c * q[i];
        }
        const double nn = std::sqrt (inner (p, p));
        for (auto& x : p) x /= nn;
        ortho.push_back (p);
    }
    // v(x) = sum p_i(1) p_i(x)
    Poly v ((size_t) deg + 1, 0.0);
    for (const auto& q : ortho)
    {
        double at1 = 0.0;
        for (auto x : q) at1 += x;
        for (size_t i = 0; i < q.size(); ++i) v[i] += at1 * q[i];
    }
    Poly integrand = polyMul (v, v);
    if (even) integrand = polyMul (integrand, Poly { 1.0, 1.0 });
    // Stammfunktion Q
    Poly Q (integrand.size() + 1, 0.0);
    for (size_t i = 0; i < integrand.size(); ++i) Q[i + 1] = integrand[i] / (double) (i + 1);
    auto evalQ = [&] (double x) { double s = 0.0, p = 1.0; for (auto cq : Q) { s += cq * p; p *= x; } return s; };
    const double q0 = evalQ (-1.0);
    // L(u) = Q(2u - 1) - Q(-1): als Polynom in u
    Poly L (Q.size(), 0.0);
    Poly powT { 1.0 }; // (2u - 1)^i
    for (size_t i = 0; i < Q.size(); ++i)
    {
        for (size_t k = 0; k < powT.size(); ++k) L[k] += Q[i] * powT[k];
        powT = polyMul (powT, Poly { -1.0, 2.0 });
    }
    L[0] -= q0;
    // Normierung L(1) = 1
    double at1 = 0.0;
    for (auto x : L) at1 += x;
    for (auto& x : L) x /= at1;
    L[0] = 0.0;
    return L;
}

inline Zpk legendre (int n)
{
    if (n <= 2) { auto z = butterworth (n); return z; }
    Poly d = legendreL (n);
    d[0] += 1.0;
    Zpk z;
    z.poles = polesFromMagSquaredDen (d);
    normaliseCutoff (z, std::sqrt (0.5));
    return z;
}

// Prototyp fuer Typ und Ordnung (Ordnung = Steilheit / 6 dB). LR: zwei Butterworth halber Ordnung.
inline Zpk prototype (XoverType t, int order)
{
    order = std::clamp (order, 1, 16);
    switch (t)
    {
        case XoverType::linkwitzRiley:
        {
            auto b = butterworth (std::max (1, order / 2));
            Zpk z;
            z.poles = b.poles;
            z.poles.insert (z.poles.end(), b.poles.begin(), b.poles.end());
            setDcGain (z);
            return z;
        }
        case XoverType::chebyshev1: return chebyshev1 (order);
        case XoverType::chebyshev2: return chebyshev2 (order);
        case XoverType::elliptic:   return elliptic (order);
        case XoverType::bessel:     return bessel (order);
        case XoverType::legendre:   return legendre (order);
        case XoverType::gaussian:   return gaussian (order);
        case XoverType::butterworth:
        case XoverType::count:
        default:                    return butterworth (order);
    }
}

// Tiefpass -> Hochpass (s -> 1/s)
inline Zpk toHighpass (const Zpk& lp)
{
    Zpk hp;
    cd k (lp.gain, 0.0);
    for (auto p : lp.poles) { hp.poles.push_back (1.0 / p); k /= -p; }
    for (auto z : lp.zeros) { hp.zeros.push_back (1.0 / z); k *= -z; }
    const int extra = (int) lp.poles.size() - (int) lp.zeros.size();
    for (int i = 0; i < extra; ++i) hp.zeros.push_back (cd (0.0, 0.0));
    hp.gain = k.real();
    // Verstaerkung so, dass |H(j inf)| = |H_lp(0)| (= 1)
    const double m = hp.mag (1e6);
    if (m > 0.0) hp.gain *= 1.0 / m;
    return hp;
}

// ------------------------------------------------------------------ Digitale Biquads
struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

struct SosFilter
{
    static constexpr int kMaxSections = 8;
    std::array<BiquadCoeffs, kMaxSections> c {};
    int numSections = 0;
};

// Bilineare Transformation eines analogen ZPK mit Vorverzerrung auf fc
inline SosFilter bilinear (const Zpk& proto, double fc, double fs)
{
    const double wc = 2.0 * fs * std::tan (kPi * std::min (fc, fs * 0.49) / fs); // analoges w fuer fc
    const double T2 = 2.0 * fs;
    // Pole/Nullstellen auf wc skalieren und abbilden: z = (T2 + s) / (T2 - s)
    std::vector<cd> zp, zz;
    cd k (proto.gain, 0.0);
    const int np = (int) proto.poles.size(), nz = (int) proto.zeros.size();
    for (auto p : proto.poles) { const cd s = p * wc; zp.push_back ((T2 + s) / (T2 - s)); k /= (T2 - s); }
    for (auto z : proto.zeros) { const cd s = z * wc; zz.push_back ((T2 + s) / (T2 - s)); k *= (T2 - s); }
    // Verstaerkung der analogen Funktion: H(s) = g * prod(s - z wc) / prod(s - p wc) mit g' = g * wc^(np - nz)
    k *= std::pow (wc, (double) (np - nz));
    for (int i = nz; i < np; ++i) zz.push_back (cd (-1.0, 0.0)); // Nullstellen im Unendlichen -> z = -1

    // Paare bilden: konjugierte Pole zusammen, reale Pole paarweise
    auto pairUp = [] (std::vector<cd> v)
    {
        std::vector<std::pair<cd, cd>> out;
        std::vector<cd> reals, cplx;
        for (auto x : v) (std::abs (x.imag()) < 1e-9 ? reals : cplx).push_back (x);
        // komplexe: nur positive Imaginaerteile nehmen
        std::vector<cd> pos;
        for (auto x : cplx) if (x.imag() > 0) pos.push_back (x);
        // nach Naehe zum Einheitskreis sortieren (stabilste Reihenfolge)
        std::sort (pos.begin(), pos.end(), [] (cd a, cd b) { return std::abs (a) < std::abs (b); });
        for (auto x : pos) out.push_back ({ x, std::conj (x) });
        std::sort (reals.begin(), reals.end(), [] (cd a, cd b) { return a.real() < b.real(); });
        for (size_t i = 0; i < reals.size(); i += 2)
            out.push_back ({ reals[i], i + 1 < reals.size() ? reals[i + 1] : cd (0.0, 0.0) });
        return out;
    };
    auto pp = pairUp (zp);
    auto zpairs = pairUp (zz);
    // reale Einzelpole: zweites Element 0 -> erster Ordnung (Nenner 1 - p z^-1)
    const bool oddPoles = (np % 2) == 1;

    SosFilter f;
    f.numSections = (int) pp.size();
    for (size_t i = 0; i < pp.size() && i < (size_t) SosFilter::kMaxSections; ++i)
    {
        auto& s = f.c[i];
        const cd p1 = pp[i].first, p2 = pp[i].second;
        const bool firstOrder = oddPoles && i == pp.size() - 1 && std::abs (p2) < 1e-300 && std::abs (p1.imag()) < 1e-9;
        s.a1 = -(p1 + p2).real();
        s.a2 = (p1 * p2).real();
        cd z1 (-1.0, 0.0), z2 (-1.0, 0.0);
        if (i < zpairs.size()) { z1 = zpairs[i].first; z2 = zpairs[i].second; }
        if (firstOrder) { z2 = cd (0.0, 0.0); s.a2 = 0.0; }
        s.b0 = 1.0;
        s.b1 = -(z1 + z2).real();
        s.b2 = (z1 * z2).real();
    }
    // Gesamtverstaerkung auf die erste Sektion legen (Betrag passend zu analog bei kleiner Frequenz bzw. Durchlass)
    // Robuster: Betrag des Digitalfilters an einer Referenzfrequenz an den des analogen anpassen
    auto digMag = [&] (double f0)
    {
        const cd zi = std::polar (1.0, -2.0 * kPi * f0 / fs);
        cd h (1.0, 0.0);
        for (int i = 0; i < f.numSections; ++i)
        {
            const auto& s = f.c[(size_t) i];
            h *= (s.b0 + s.b1 * zi + s.b2 * zi * zi) / (1.0 + s.a1 * zi + s.a2 * zi * zi);
        }
        return std::abs (h);
    };
    // Referenz: Frequenz, bei der das analoge Filter seinen Durchlass hat (Tiefpass: tief, Hochpass: hoch)
    const bool isHp = proto.zeros.size() == proto.poles.size() && proto.mag (1e-4) < 0.1;
    const double fRef = isHp ? fs * 0.49 : 1.0;
    const double wRefA = 2.0 * fs * std::tan (kPi * fRef / fs) / wc;
    const double target = proto.mag (wRefA);
    const double cur = digMag (fRef);
    const double g = cur > 0.0 ? target / cur : 1.0;
    if (f.numSections > 0) { f.c[0].b0 *= g; f.c[0].b1 *= g; f.c[0].b2 *= g; }
    (void) k;
    return f;
}

// Frequenzgang eines digitalen Filters (komplex)
inline cd sosResponse (const SosFilter& f, double freq, double fs)
{
    const cd zi = std::polar (1.0, -2.0 * kPi * freq / fs);
    cd h (1.0, 0.0);
    for (int i = 0; i < f.numSections; ++i)
    {
        const auto& s = f.c[(size_t) i];
        h *= (s.b0 + s.b1 * zi + s.b2 * zi * zi) / (1.0 + s.a1 * zi + s.a2 * zi * zi);
    }
    return h;
}

// Laufzeit-Filter (TDF-II, double), bis 8 Sektionen, Koeffizienten im laufenden Betrieb tauschbar
template <int Channels>
struct SosRunner
{
    SosFilter f;
    double gainSign = 1.0;
    std::array<std::array<std::array<double, 2>, SosFilter::kMaxSections>, (size_t) Channels> z {};

    void reset() { for (auto& ch : z) for (auto& s : ch) s = { 0.0, 0.0 }; }

    inline double process (int ch, double x)
    {
        auto& st = z[(size_t) ch];
        for (int i = 0; i < f.numSections; ++i)
        {
            const auto& c = f.c[(size_t) i];
            auto& s = st[(size_t) i];
            const double y = c.b0 * x + s[0];
            s[0] = c.b1 * x - c.a1 * y + s[1];
            s[1] = c.b2 * x - c.a2 * y;
            x = y;
        }
        return x * gainSign;
    }
};

} // namespace bfz
