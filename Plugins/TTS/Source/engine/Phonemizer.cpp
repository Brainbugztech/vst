#include "Phonemizer.h"
#include "Utf.h"
#include "NumberWords.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <tuple>

namespace tts
{
namespace
{
const std::u32string VOWELS = U"AIOQWYaiuæɑɒɔəɛɜɪʊʌᵻ";
const std::u32string CONSONANTS = U"bdfhjklmnpstvwzðŋɡɹɾʃʒʤʧθ";
const std::u32string DIPHTHONGS = U"AIOQWYʤʧ";
const std::u32string PUNCTS = U";:,.!?—…\"“”";
const std::u32string NON_QUOTE_PUNCTS = U";:,.!?—…";
const std::u32string US_TAUS = U"AIOWYiuæɑəɛɪɹʊʌ";
constexpr char32_t PRIMARY = U'ˈ';
constexpr char32_t SECONDARY = U'ˌ';

bool has (const std::u32string& set, char32_t c) { return set.find (c) != std::u32string::npos; }
bool hasAny (const std::u32string& s, const std::u32string& set)
{
    for (auto c : s) if (has (set, c)) return true;
    return false;
}
bool isAlphaWord (const std::u32string& s)
{
    if (s.empty()) return false;
    for (auto c : s) if (! isLetter (c)) return false;
    return true;
}
bool isDigits (const std::u32string& s)
{
    if (s.empty()) return false;
    for (auto c : s) if (! isAsciiDigit (c)) return false;
    return true;
}
bool endsWith (const std::u32string& s, const std::u32string& e)
{
    return s.size() >= e.size() && s.compare (s.size() - e.size(), e.size(), e) == 0;
}
void replaceAll (std::u32string& s, char32_t from, const std::u32string& to)
{
    std::u32string out;
    for (auto c : s) { if (c == from) out += to; else out.push_back (c); }
    s = out;
}
void removeChar (std::u32string& s, char32_t c) { s.erase (std::remove (s.begin(), s.end(), c), s.end()); }

int stressWeight (const std::u32string& ps)
{
    int w = 0;
    for (auto c : ps) w += has (DIPHTHONGS, c) ? 2 : 1;
    return w;
}
} // namespace

// ---------------------------------------------------------------- Laden

Phonemizer::Phonemizer (const std::string& lexiconText, bool isBritish) : british (isBritish)
{
    load (lexiconText);
    grow (golds);
    grow (silvers);
}

void Phonemizer::load (const std::string& text)
{
    std::unordered_map<std::u32string, Entry>* target = &golds;
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t end = text.find ('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr (pos, end - pos);
        pos = end + 1;
        if (! line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line == "#gold")   { target = &golds; continue; }
        if (line == "#silver") { target = &silvers; continue; }
        const auto tab = line.find ('\t');
        if (tab == std::string::npos) continue;
        const auto key = toU32 (line.substr (0, tab));
        const auto val = line.substr (tab + 1);
        Entry e;
        if (! val.empty() && val[0] == '@')
        {
            e.tagged = true;
            std::stringstream ss (val.substr (1));
            std::string part;
            while (std::getline (ss, part, ';'))
            {
                const auto eq = part.find ('=');
                if (eq == std::string::npos) continue;
                const auto tag = part.substr (0, eq);
                const auto ps = part.substr (eq + 1);
                if (ps == "~") e.tags.push_back ({ tag, std::nullopt });
                else e.tags.push_back ({ tag, toU32 (ps) });
            }
        }
        else
            e.ps = toU32 (val);
        (*target)[key] = std::move (e);
    }
}

void Phonemizer::grow (std::unordered_map<std::u32string, Entry>& d)
{
    std::vector<std::pair<std::u32string, Entry>> extra;
    for (auto& [k, v] : d)
    {
        if (k.size() < 2) continue;
        if (k == lower (k))
        {
            const auto cap = capitalize (k);
            if (cap != k && d.find (cap) == d.end()) extra.push_back ({ cap, v });
        }
        else if (k == capitalize (k))
        {
            const auto lo = lower (k);
            if (d.find (lo) == d.end()) extra.push_back ({ lo, v });
        }
    }
    for (auto& e : extra) d.emplace (std::move (e.first), std::move (e.second));
}

// ---------------------------------------------------------------- Betonung

std::u32string Phonemizer::applyStress (std::u32string ps, Stress stress)
{
    auto restress = [] (const std::u32string& p)
    {
        // Betonungszeichen direkt vor den nächsten Vokal schieben
        std::vector<std::pair<double, char32_t>> ips;
        for (size_t i = 0; i < p.size(); ++i) ips.push_back ({ (double) i, p[i] });
        for (size_t i = 0; i < p.size(); ++i)
        {
            if (p[i] != PRIMARY && p[i] != SECONDARY) continue;
            for (size_t j = i; j < p.size(); ++j)
                if (has (VOWELS, p[j])) { ips[i].first = (double) j - 0.5; break; }
        }
        std::stable_sort (ips.begin(), ips.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        std::u32string out;
        for (auto& x : ips) out.push_back (x.second);
        return out;
    };
    if (! stress.has_value()) return ps;
    const double s = *stress;
    const bool hasP = ps.find (PRIMARY) != std::u32string::npos;
    const bool hasS = ps.find (SECONDARY) != std::u32string::npos;
    if (s < -1)
    {
        removeChar (ps, PRIMARY);
        removeChar (ps, SECONDARY);
        return ps;
    }
    if (s == -1 || ((s == 0 || s == -0.5) && hasP))
    {
        removeChar (ps, SECONDARY);
        std::replace (ps.begin(), ps.end(), PRIMARY, SECONDARY);
        return ps;
    }
    if ((s == 0 || s == 0.5 || s == 1) && ! hasP && ! hasS)
    {
        if (! hasAny (ps, VOWELS)) return ps;
        return restress (std::u32string (1, SECONDARY) + ps);
    }
    if (s >= 1 && ! hasP && hasS)
    {
        std::replace (ps.begin(), ps.end(), SECONDARY, PRIMARY);
        return ps;
    }
    if (s > 1 && ! hasP && ! hasS)
    {
        if (! hasAny (ps, VOWELS)) return ps;
        return restress (std::u32string (1, PRIMARY) + ps);
    }
    return ps;
}

// ---------------------------------------------------------------- Lexikon

Phonemizer::Result Phonemizer::getNNP (const std::u32string& word) const
{
    std::u32string ps;
    for (auto c : word)
    {
        if (! isLetter (c)) continue;
        auto it = golds.find (std::u32string (1, upperChar (c)));
        if (it == golds.end() || it->second.tagged) return {};
        ps += it->second.ps;
    }
    if (ps.empty()) return {};
    ps = applyStress (ps, 0.0);
    // letzte Nebenbetonung wird Hauptbetonung
    const auto at = ps.rfind (SECONDARY);
    if (at != std::u32string::npos) ps[at] = PRIMARY;
    return { ps, 3 };
}

Phonemizer::Result Phonemizer::lookup (std::u32string word, const std::string& tag, Stress stress, const Ctx* ctx) const
{
    bool isNNP = false;
    if (word == upper (word) && golds.find (word) == golds.end())
        word = lower (word);
    const Entry* e = nullptr;
    int rating = 4;
    if (auto it = golds.find (word); it != golds.end()) e = &it->second;
    else if (auto it2 = silvers.find (word); it2 != silvers.end()) { e = &it2->second; rating = 3; }
    std::optional<std::u32string> ps;
    if (e != nullptr)
    {
        if (! e->tagged) ps = e->ps;
        else
        {
            std::string want = tag;
            auto find = [&] (const std::string& t) -> const std::optional<std::u32string>*
            {
                for (auto& p : e->tags) if (p.first == t) return &p.second;
                return nullptr;
            };
            if (ctx != nullptr && ! ctx->futureVowel.has_value() && find ("None") != nullptr) want = "None";
            const std::optional<std::u32string>* hit = want.empty() ? nullptr : find (want);
            if (hit == nullptr) hit = find ("DEFAULT");
            if (hit != nullptr) ps = *hit;
        }
    }
    if (! ps.has_value() || (isNNP && ps->find (PRIMARY) == std::u32string::npos))
    {
        auto r = getNNP (word);
        if (r.ps) return r;
    }
    if (! ps.has_value()) return {};
    return { applyStress (*ps, stress), rating };
}

Phonemizer::Result Phonemizer::specialCase (const std::u32string& word, const std::string&, Stress stress, const Ctx& ctx) const
{
    static const std::map<std::u32string, const char32_t*> symbols = {
        { U"%", U"percent" }, { U"&", U"and" }, { U"+", U"plus" }, { U"@", U"at" } };
    if (auto it = symbols.find (word); it != symbols.end())
        return lookup (it->second, "", std::nullopt, &ctx);

    // Abkürzungen mit Punkten (U.S.)
    {
        std::u32string stripped = word;
        while (! stripped.empty() && stripped.front() == '.') stripped.erase (stripped.begin());
        while (! stripped.empty() && stripped.back() == '.') stripped.pop_back();
        if (stripped.find ('.') != std::u32string::npos)
        {
            std::u32string letters;
            size_t longest = 0, cur = 0;
            bool ok = true;
            for (auto c : word)
            {
                if (c == '.') { longest = std::max (longest, cur); cur = 0; continue; }
                if (! isLetter (c)) { ok = false; break; }
                letters.push_back (c);
                ++cur;
            }
            longest = std::max (longest, cur);
            if (ok && ! letters.empty() && longest < 3) return getNNP (word);
        }
    }
    if (word == U"a" || word == U"A") return { std::u32string (U"ɐ"), 4 };
    if (word == U"am" || word == U"Am" || word == U"AM")
    {
        if (! ctx.futureVowel.has_value() || word != U"am" || (stress && *stress > 0))
            return lookup (U"am", "", std::nullopt, nullptr);
        return { std::u32string (U"ɐm"), 4 };
    }
    if (word == U"an" || word == U"An" || word == U"AN") return { std::u32string (U"ɐn"), 4 };
    if (word == U"I") return { std::u32string (U"ˌI"), 4 };
    if (word == U"to" || word == U"To" || word == U"TO")
    {
        if (! ctx.futureVowel.has_value()) return lookup (U"to", "", std::nullopt, nullptr);
        return { std::u32string (*ctx.futureVowel ? U"tʊ" : U"tə"), 4 };
    }
    if (word == U"in" || word == U"In" || word == U"IN")
        return { std::u32string (ctx.futureVowel.has_value() ? U"ɪn" : U"ˈɪn"), 4 };
    if (word == U"the" || word == U"The" || word == U"THE")
        return { std::u32string ((ctx.futureVowel.has_value() && *ctx.futureVowel) ? U"ði" : U"ðə"), 4 };
    if (lower (word) == U"vs" || lower (word) == U"vs.")
        return lookup (U"versus", "", std::nullopt, &ctx);
    if (word == U"used" || word == U"Used" || word == U"USED")
        return lookup (U"used", ctx.futureTo ? "VBD" : "DEFAULT", std::nullopt, nullptr);
    return {};
}

bool Phonemizer::isKnown (const std::u32string& word) const
{
    if (golds.count (word) || silvers.count (word)) return true;
    if (word == U"%" || word == U"&" || word == U"+" || word == U"@") return true;
    if (! isAlphaWord (word)) return false;
    for (auto c : word) if (! isAsciiAlpha (c)) return false;
    if (word.size() == 1) return true;
    if (word == upper (word) && golds.count (lower (word))) return true;
    const auto rest = word.substr (1);
    return rest == upper (rest);
}

std::optional<std::u32string> Phonemizer::suffixS (const std::u32string& stem) const
{
    if (stem.empty()) return std::nullopt;
    const auto last = stem.back();
    if (has (U"ptkfθ", last)) return stem + U"s";
    if (has (U"szʃʒʧʤ", last)) return stem + (british ? U"ɪz" : U"ᵻz");
    return stem + U"z";
}

std::optional<std::u32string> Phonemizer::suffixEd (const std::u32string& stem) const
{
    if (stem.empty()) return std::nullopt;
    const auto last = stem.back();
    if (has (U"pkfθʃsʧ", last)) return stem + U"t";
    if (last == U'd') return stem + (british ? U"ɪd" : U"ᵻd");
    if (last != U't') return stem + U"d";
    if (british || stem.size() < 2) return stem + U"ɪd";
    if (has (US_TAUS, stem[stem.size() - 2])) return stem.substr (0, stem.size() - 1) + U"ɾᵻd";
    return stem + U"ᵻd";
}

std::optional<std::u32string> Phonemizer::suffixIng (const std::u32string& stem) const
{
    if (stem.empty()) return std::nullopt;
    if (british)
    {
        if (has (U"əː", stem.back())) return std::nullopt;
    }
    else if (stem.size() > 1 && stem.back() == U't' && has (US_TAUS, stem[stem.size() - 2]))
        return stem.substr (0, stem.size() - 1) + U"ɾɪŋ";
    return stem + U"ɪŋ";
}

Phonemizer::Result Phonemizer::stemS (const std::u32string& word, Stress stress, const Ctx* ctx) const
{
    if (word.size() < 3 || word.back() != U's') return {};
    std::u32string stem;
    if (! endsWith (word, U"ss") && isKnown (word.substr (0, word.size() - 1)))
        stem = word.substr (0, word.size() - 1);
    else if ((endsWith (word, U"'s") || (word.size() > 4 && endsWith (word, U"es") && ! endsWith (word, U"ies")))
             && isKnown (word.substr (0, word.size() - 2)))
        stem = word.substr (0, word.size() - 2);
    else if (word.size() > 4 && endsWith (word, U"ies") && isKnown (word.substr (0, word.size() - 3) + U"y"))
        stem = word.substr (0, word.size() - 3) + U"y";
    else
        return {};
    auto r = lookup (stem, "", stress, ctx);
    if (! r.ps) return {};
    return { suffixS (*r.ps), r.rating };
}

Phonemizer::Result Phonemizer::stemEd (const std::u32string& word, Stress stress, const Ctx* ctx) const
{
    if (word.size() < 4 || word.back() != U'd') return {};
    std::u32string stem;
    if (! endsWith (word, U"dd") && isKnown (word.substr (0, word.size() - 1)))
        stem = word.substr (0, word.size() - 1);
    else if (word.size() > 4 && endsWith (word, U"ed") && ! endsWith (word, U"eed") && isKnown (word.substr (0, word.size() - 2)))
        stem = word.substr (0, word.size() - 2);
    else
        return {};
    auto r = lookup (stem, "", stress, ctx);
    if (! r.ps) return {};
    return { suffixEd (*r.ps), r.rating };
}

Phonemizer::Result Phonemizer::stemIng (const std::u32string& word, Stress stress, const Ctx* ctx) const
{
    if (word.size() < 5 || ! endsWith (word, U"ing")) return {};
    const auto base = word.substr (0, word.size() - 3);
    std::u32string stem;
    auto doubled = [&]
    {
        if (word.size() < 6) return false;
        if (endsWith (word, U"cking")) return true;
        const auto a = word[word.size() - 4], b = word[word.size() - 5];
        return a == b && has (U"bcdgklmnprstvxz", a);
    };
    if (word.size() > 5 && isKnown (base)) stem = base;
    else if (isKnown (base + U"e")) stem = base + U"e";
    else if (word.size() > 5 && doubled() && isKnown (word.substr (0, word.size() - 4))) stem = word.substr (0, word.size() - 4);
    else return {};
    auto r = lookup (stem, "", stress, ctx);
    if (! r.ps) return {};
    return { suffixIng (*r.ps), r.rating };
}

Phonemizer::Result Phonemizer::getWord (std::u32string word, const std::string& tag, Stress stress, const Ctx& ctx) const
{
    // Musik-Kontext: "bass" ist der Bass, nicht der Fisch
    static const std::map<std::u32string, const char32_t*> overrides = { { U"bass", U"bˈAs" }, { U"basses", U"bˈAsᵻz" } };
    if (auto it = overrides.find (lower (word)); it != overrides.end())
        return { applyStress (it->second, stress), 4 };
    if (auto r = specialCase (word, tag, stress, ctx); r.ps) return r;
    const auto wl = lower (word);
    std::u32string noApos = word;
    removeChar (noApos, U'\'');
    if (word.size() > 1 && isAlphaWord (noApos) && word != wl
        && ! golds.count (word) && ! silvers.count (word)
        && (word == upper (word) || word.substr (1) == lower (word.substr (1)))
        && (golds.count (wl) || silvers.count (wl) || stemS (wl, stress, &ctx).ps || stemEd (wl, stress, &ctx).ps
            || stemIng (wl, stress, &ctx).ps))
        word = wl;
    if (isKnown (word)) return lookup (word, tag, stress, &ctx);
    if (endsWith (word, U"s'") && isKnown (word.substr (0, word.size() - 2) + U"'s"))
        return lookup (word.substr (0, word.size() - 2) + U"'s", tag, stress, &ctx);
    if (endsWith (word, U"'") && isKnown (word.substr (0, word.size() - 1)))
        return lookup (word.substr (0, word.size() - 1), tag, stress, &ctx);
    if (auto r = stemS (word, stress, &ctx); r.ps) return r;
    if (auto r = stemEd (word, stress, &ctx); r.ps) return r;
    if (auto r = stemIng (word, stress.has_value() ? stress : Stress (0.5), &ctx); r.ps) return r;
    return {};
}

// ---------------------------------------------------------------- Zahlen

Phonemizer::Result Phonemizer::getNumber (std::u32string word, char32_t currency, bool isHead) const
{
    std::u32string suffix;
    while (! word.empty() && (isAsciiLower (word.back()) || word.back() == U'\''))
    {
        suffix.insert (suffix.begin(), word.back());
        word.pop_back();
    }
    std::vector<std::u32string> parts;
    if (const auto colon = word.find (U':'); colon != std::u32string::npos)
    {
        // Uhrzeit 10:30 -> ten thirty, 10:05 -> ten O five, 10:00 -> ten o'clock
        uint64_t h = 0, m = 0;
        for (auto c : word.substr (0, colon)) if (isAsciiDigit (c)) h = h * 10 + (uint64_t) (c - '0');
        for (auto c : word.substr (colon + 1)) if (isAsciiDigit (c)) m = m * 10 + (uint64_t) (c - '0');
        std::vector<std::string> ws = numwords::cardinal (h);
        if (m == 0) ws.push_back ("o'clock");
        else if (m < 10) { ws.push_back ("O"); ws.push_back (numwords::ones[m]); }
        else for (auto& x : numwords::cardinal (m)) ws.push_back (x);
        std::u32string ps;
        for (auto& x : ws)
        {
            auto r = lookup (toU32 (x), "", x == "O" ? Stress (-2.0) : std::nullopt, nullptr);
            if (! ps.empty()) ps += U" ";
            ps += r.ps ? *r.ps : fallback (toU32 (x));
        }
        return { ps, 4 };
    }
    auto addWords = [&] (const std::vector<std::string>& ws) { for (auto& w : ws) parts.push_back (toU32 (w)); };
    if (! word.empty() && word[0] == U'-') { parts.push_back (U"minus"); word.erase (word.begin()); }
    std::u32string plain;
    for (auto c : word) if (c != U',') plain.push_back (c);
    auto toNum = [] (const std::u32string& s) -> uint64_t
    {
        uint64_t n = 0;
        for (auto c : s) { if (! isAsciiDigit (c)) break; n = n * 10 + (uint64_t) (c - '0'); if (n > 999999999999999ull) break; }
        return n;
    };
    auto digitsOneByOne = [&] (const std::u32string& s) { for (auto c : s) if (isAsciiDigit (c)) parts.push_back (toU32 (numwords::ones[c - '0'])); };
    const bool ordinalSuffix = suffix == U"st" || suffix == U"nd" || suffix == U"rd" || suffix == U"th";
    static const std::map<char32_t, std::pair<const char32_t*, const char32_t*>> currencies = {
        { U'$', { U"dollar", U"cent" } }, { U'£', { U"pound", U"pence" } }, { U'€', { U"euro", U"cent" } } };

    if (isDigits (word) && ordinalSuffix)
        addWords (numwords::ordinal (toNum (word)));
    else if (parts.empty() && word.size() == 4 && currency == 0 && isDigits (word))
        addWords (numwords::year ((int) toNum (word)));
    else if (! isHead && word.find (U'.') == std::u32string::npos)
    {
        if (plain.empty()) return {};
        if (plain[0] == U'0' || plain.size() > 3) digitsOneByOne (plain);
        else addWords (numwords::cardinal (toNum (plain)));
    }
    else if (currency != 0 && currencies.count (currency))
    {
        const auto& names = currencies.at (currency);
        const auto dot = plain.find (U'.');
        const uint64_t major = toNum (plain.substr (0, dot));
        uint64_t minor = 0;
        if (dot != std::u32string::npos)
        {
            auto c = plain.substr (dot + 1);
            if (c.size() == 1) c += U"0";
            minor = toNum (c.substr (0, 2));
        }
        auto unit = [&] (uint64_t n, const std::u32string& name)
        {
            addWords (numwords::cardinal (n));
            if (n != 1 && name != U"pence") parts.push_back (name + U"s");
            else parts.push_back (name);
        };
        if (major > 0 || minor == 0) unit (major, names.first);
        if (minor > 0)
        {
            if (major > 0) parts.push_back (U"and");
            unit (minor, names.second);
        }
    }
    else if (std::count (word.begin(), word.end(), U'.') == 1)
    {
        const auto dot = plain.find (U'.');
        if (dot > 0) addWords (numwords::cardinal (toNum (plain.substr (0, dot))));
        parts.push_back (U"point");
        digitsOneByOne (plain.substr (dot + 1));
    }
    else if (std::count (word.begin(), word.end(), U'.') > 1)
    {
        // Versionsnummern / IPs: Teile einzeln
        std::u32string cur;
        for (auto c : plain + U".")
        {
            if (c == U'.') { if (! cur.empty()) addWords (numwords::cardinal (toNum (cur))); cur.clear(); parts.push_back (U"dot"); }
            else cur.push_back (c);
        }
        if (! parts.empty() && parts.back() == U"dot") parts.pop_back();
    }
    else
    {
        if (ordinalSuffix) addWords (numwords::ordinal (toNum (plain)));
        else addWords (numwords::cardinal (toNum (plain)));
    }
    if (parts.empty()) return {};

    std::u32string ps;
    int rating = 4;
    for (auto& p : parts)
    {
        auto r = lookup (p, "", p == U"point" ? Stress (-2.0) : (p == U"O" ? Stress (-2.0) : std::nullopt), nullptr);
        if (! r.ps) { auto fb = fallback (p); r.ps = fb; r.rating = 1; }
        if (! ps.empty()) ps += U" ";
        ps += *r.ps;
        rating = std::min (rating, r.rating);
    }
    if (suffix == U"s" || suffix == U"'s") return { suffixS (ps), rating };
    if (suffix == U"ed" || suffix == U"'d") return { suffixEd (ps), rating };
    if (suffix == U"ing") return { suffixIng (ps), rating };
    return { ps, rating };
}

// ---------------------------------------------------------------- Ersatz für unbekannte Wörter

std::u32string Phonemizer::letterRules (const std::u32string& w) const
{
    // Grobe englische Ausspracheregeln für Wörter, die nicht im Wörterbuch stehen
    std::u32string out;
    const size_t n = w.size();
    auto at = [&] (size_t i) -> char32_t { return i < n ? w[i] : 0; };
    auto isV = [] (char32_t c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y'; };
    auto match = [&] (size_t i, const char32_t* s)
    {
        for (size_t k = 0; s[k] != 0; ++k) if (at (i + k) != s[k]) return false;
        return true;
    };
    const char32_t* shortA = british ? U"a" : U"æ";
    const char32_t* shortO = british ? U"ɒ" : U"ɑ";
    size_t i = 0;
    while (i < n)
    {
        const char32_t c = w[i];
        // magisches e: Vokal + ein Konsonant + e am Wortende
        if (isV (c) && c != 'y' && i + 2 == n - 1 && ! isV (at (i + 1)) && at (i + 2) == 'e' && at (i + 1) != 'r')
        {
            static const std::map<char32_t, const char32_t*> longV = { { 'a', U"A" }, { 'e', U"i" }, { 'i', U"I" }, { 'o', U"O" }, { 'u', U"u" } };
            out += longV.at (c);
            const auto cons = at (i + 1);
            out += letterRules (std::u32string (1, cons));
            i += 3;
            continue;
        }
        struct Rule { const char32_t* g; const char32_t* p; };
        static const Rule multi[] = {
            { U"tion", U"ʃən" }, { U"sion", U"ʒən" }, { U"ough", U"ɔ" }, { U"tch", U"ʧ" }, { U"sch", U"sk" }, { U"igh", U"I" },
            { U"ch", U"ʧ" }, { U"sh", U"ʃ" }, { U"th", U"θ" }, { U"ph", U"f" }, { U"wh", U"w" }, { U"ck", U"k" },
            { U"ng", U"ŋ" }, { U"qu", U"kw" }, { U"ee", U"i" }, { U"ea", U"i" }, { U"oo", U"u" }, { U"ou", U"W" },
            { U"oi", U"Y" }, { U"oy", U"Y" }, { U"ai", U"A" }, { U"ay", U"A" }, { U"ei", U"A" }, { U"ey", U"A" },
            { U"au", U"ɔ" }, { U"aw", U"ɔ" }, { U"oa", U"O" }, { U"ie", U"i" }, { U"ue", U"u" }, { U"ew", U"u" },
            { U"zz", U"z" } };
        bool done = false;
        if (i == 0 && (match (0, U"kn") || match (0, U"gn"))) { out += U"n"; i = 2; continue; }
        if (i == 0 && match (0, U"wr")) { out += U"ɹ"; i = 2; continue; }
        if (match (i, U"ow")) { out += (i + 2 == n) ? U"O" : U"W"; i += 2; continue; }
        if (match (i, U"gh")) { if (i == 0) out += U"ɡ"; i += 2; continue; }
        for (auto& r : multi)
            if (match (i, r.g)) { out += r.p; i += std::char_traits<char32_t>::length (r.g); done = true; break; }
        if (done) continue;
        // r nach Vokal
        if (isV (c) && at (i + 1) == 'r' && ! isV (at (i + 2)))
        {
            if (british)
            {
                static const std::map<char32_t, const char32_t*> gbR = { { 'a', U"ɑː" }, { 'e', U"ɜː" }, { 'i', U"ɜː" }, { 'o', U"ɔː" }, { 'u', U"ɜː" }, { 'y', U"ɜː" } };
                out += (c == 'e' && i + 2 >= n) ? U"ə" : gbR.at (c);
            }
            else
            {
                static const std::map<char32_t, const char32_t*> usR = { { 'a', U"ɑɹ" }, { 'e', U"ɜɹ" }, { 'i', U"ɜɹ" }, { 'o', U"ɔɹ" }, { 'u', U"ɜɹ" }, { 'y', U"ɜɹ" } };
                out += (c == 'e' && i + 2 >= n) ? U"əɹ" : usR.at (c);
            }
            i += 2;
            continue;
        }
        // Doppelkonsonanten zusammenfassen
        if (! isV (c) && at (i + 1) == c) { ++i; continue; }
        switch (c)
        {
            case 'a': out += shortA; break;
            case 'e': if (! (i == n - 1 && n > 2)) out += U"ɛ"; break; // stummes e am Ende
            case 'i': out += U"ɪ"; break;
            case 'o': out += shortO; break;
            case 'u': out += U"ʌ"; break;
            case 'y':
                if (i == 0 && isV (at (1))) out += U"j";
                else if (i == n - 1) out += U"i";
                else out += U"ɪ";
                break;
            case 'c': out += (at (i + 1) == 'e' || at (i + 1) == 'i' || at (i + 1) == 'y') ? U"s" : U"k"; break;
            case 'g': out += (i + 2 == n && at (i + 1) == 'e') ? U"ʤ" : U"ɡ"; break;
            case 'j': out += U"ʤ"; break;
            case 'q': out += U"k"; break;
            case 'r': out += U"ɹ"; break;
            case 'x': out += (i == 0) ? U"z" : U"ks"; break;
            case 'b': case 'd': case 'f': case 'h': case 'k': case 'l': case 'm': case 'n':
            case 'p': case 's': case 't': case 'v': case 'w': case 'z':
                out.push_back (c);
                break;
            default:
                break; // Akzente/Sonstiges: weglassen
        }
        ++i;
    }
    return out;
}

std::u32string Phonemizer::fallback (const std::u32string& word) const
{
    // 1) Wort in bekannte Teilwörter zerlegen (Brainbugz -> brain + bugz), Rest über Buchstabenregeln
    std::u32string w;
    for (auto c : lower (word)) if (isAsciiAlpha (c)) w.push_back (c);
    if (w.empty()) return {};

    std::vector<std::u32string> segs;
    std::u32string unknownRun;
    auto flushRun = [&]
    {
        if (unknownRun.empty()) return;
        segs.push_back (letterRules (unknownRun));
        unknownRun.clear();
    };
    size_t i = 0;
    while (i < w.size())
    {
        size_t best = 0;
        std::u32string bestPs;
        for (size_t len = std::min<size_t> (w.size() - i, 14); len >= 3; --len)
        {
            if (i == 0 && len == w.size()) continue; // ganzes Wort war schon unbekannt
            const auto sub = w.substr (i, len);
            auto it = golds.find (sub);
            const Entry* e = it != golds.end() ? &it->second : nullptr;
            if (e == nullptr) { auto it2 = silvers.find (sub); if (it2 != silvers.end()) e = &it2->second; }
            if (e == nullptr) continue;
            std::optional<std::u32string> ps;
            if (! e->tagged) ps = e->ps;
            else for (auto& t : e->tags) if (t.first == "DEFAULT") ps = t.second;
            if (! ps || ps->empty()) continue;
            best = len;
            bestPs = *ps;
            break;
        }
        if (best > 0)
        {
            flushRun();
            segs.push_back (bestPs);
            i += best;
        }
        else
            unknownRun.push_back (w[i++]);
    }
    flushRun();

    // 2) zusammensetzen: nur die erste Hauptbetonung bleibt, weitere werden Nebenbetonung
    std::u32string ps;
    for (auto& s : segs) ps += s;
    bool seenPrimary = false;
    for (auto& c : ps)
    {
        if (c == PRIMARY)
        {
            if (seenPrimary) c = SECONDARY;
            seenPrimary = true;
        }
    }
    if (! seenPrimary)
    {
        for (size_t k = 0; k < ps.size(); ++k)
            if (has (VOWELS, ps[k])) { ps.insert (ps.begin() + (long) k, PRIMARY); break; }
    }
    return ps;
}

// ---------------------------------------------------------------- Text -> Phoneme

Phonemizer::Ctx Phonemizer::nextCtx (const Ctx& ctx, const std::u32string& ps, const std::u32string& word)
{
    Ctx out;
    out.futureVowel = ctx.futureVowel;
    for (auto c : ps)
    {
        if (has (NON_QUOTE_PUNCTS, c)) { out.futureVowel = std::nullopt; break; }
        if (has (VOWELS, c)) { out.futureVowel = true; break; }
        if (has (CONSONANTS, c)) { out.futureVowel = false; break; }
    }
    out.futureTo = word == U"to" || word == U"To" || word == U"TO";
    return out;
}

Phonemizer::Result Phonemizer::wordToPhonemes (const std::u32string& rawWord, char32_t currency, bool isHead, const Ctx& ctx) const
{
    std::u32string word = rawWord;
    std::replace (word.begin(), word.end(), U'‘', U'\'');
    std::replace (word.begin(), word.end(), U'’', U'\'');
    Stress stress;
    if (word != lower (word)) stress = (word == upper (word)) ? 2.0 : 0.5;
    auto r = getWord (word, "", stress, ctx);
    if (r.ps)
    {
        if (currency != 0)
        {
            static const std::map<char32_t, const char32_t*> cur = { { U'$', U"dollars" }, { U'£', U"pounds" }, { U'€', U"euros" } };
            if (auto it = cur.find (currency); it != cur.end())
                if (auto c = lookup (it->second, "", std::nullopt, nullptr); c.ps) *r.ps += U" " + *c.ps;
        }
        return r;
    }
    bool hasDigit = false;
    for (auto c : word) if (isAsciiDigit (c)) hasDigit = true;
    if (hasDigit) return getNumber (word, currency, isHead);
    return {};
}

std::string Phonemizer::phonemize (const std::string& textIn) const
{
    // ---- Vorverarbeitung
    std::u32string text = toU32 (textIn);
    {
        std::u32string t;
        for (size_t i = 0; i < text.size(); ++i)
        {
            const auto c = text[i];
            if (c == U'.' && i + 2 < text.size() && text[i + 1] == U'.' && text[i + 2] == U'.') { t += U"…"; i += 2; continue; }
            if (c == U'-' && i + 1 < text.size() && text[i + 1] == U'-') { t += U" — "; ++i; continue; }
            if (c == U'–' || c == U'—') { t += U" — "; continue; }
            if (c == U'\n' || c == U'\r')
            {
                // Zeilenumbruch = Satzende, falls dort kein Satzzeichen steht
                size_t k = t.size();
                while (k > 0 && (t[k - 1] == U' ' || t[k - 1] == U'\t')) --k;
                if (k > 0 && ! has (PUNCTS, t[k - 1]) && t[k - 1] != U'\n') t += U".";
                t += U" ";
                continue;
            }
            if (c == U'\t') { t += U" "; continue; }
            t.push_back (c);
        }
        text = t;
    }

    // ---- Zerlegung in Gruppen (durch Leerzeichen getrennt) und Teilstücke
    struct Sub { std::u32string text; int kind; }; // 0 Wort, 1 Zahl, 2 Satzzeichen, 3 Bindestrich/Sonstiges
    struct Group { std::vector<Sub> subs; bool spaceAfter = false; };
    std::vector<Group> groups;
    {
        size_t i = 0;
        while (i < text.size())
        {
            while (i < text.size() && text[i] == U' ') ++i;
            if (i >= text.size()) break;
            Group g;
            while (i < text.size() && text[i] != U' ')
            {
                const auto c = text[i];
                if (isLetter (c))
                {
                    size_t j = i;
                    while (j < text.size() && (isLetter (text[j])
                                                || ((text[j] == U'\'' || text[j] == U'’') && j + 1 < text.size() && isLetter (text[j + 1]))))
                        ++j;
                    auto word = text.substr (i, j - i);
                    // CamelCase trennen (BrainBugz -> Brain Bugz), Großbuchstaben-Blöcke zusammen lassen
                    size_t s = 0;
                    for (size_t k = 1; k < word.size(); ++k)
                    {
                        if (isUpperLetter (word[k]) && ! isUpperLetter (word[k - 1]) && word[k - 1] != U'\'')
                        {
                            g.subs.push_back ({ word.substr (s, k - s), 0 });
                            s = k;
                        }
                        else if (isUpperLetter (word[k - 1]) && isUpperLetter (word[k]) && k + 1 < word.size()
                                 && ! isUpperLetter (word[k + 1]) && word[k + 1] != U'\'')
                        {
                            g.subs.push_back ({ word.substr (s, k - s), 0 });
                            s = k;
                        }
                    }
                    g.subs.push_back ({ word.substr (s), 0 });
                    // Suffix direkt an Zahl (1st, 90s) gehört zur Zahl
                    i = j;
                }
                else if (isAsciiDigit (c) || ((c == U'-') && g.subs.empty() && i + 1 < text.size() && isAsciiDigit (text[i + 1])))
                {
                    size_t j = i + 1;
                    while (j < text.size() && (isAsciiDigit (text[j])
                                                || ((text[j] == U',' || text[j] == U'.' || text[j] == U':') && j + 1 < text.size() && isAsciiDigit (text[j + 1]))))
                        ++j;
                    // angehängtes Suffix (st, nd, rd, th, s, 's)
                    size_t k = j;
                    while (k < text.size() && (isAsciiLower (text[k]) || text[k] == U'\'') && k - j < 3) ++k;
                    auto suf = text.substr (j, k - j);
                    if (suf == U"st" || suf == U"nd" || suf == U"rd" || suf == U"th" || suf == U"s" || suf == U"'s")
                        j = k;
                    g.subs.push_back ({ text.substr (i, j - i), 1 });
                    i = j;
                }
                else if (has (PUNCTS, c) || c == U'(' || c == U')')
                {
                    g.subs.push_back ({ std::u32string (1, c), 2 });
                    ++i;
                }
                else
                {
                    g.subs.push_back ({ std::u32string (1, c), 3 });
                    ++i;
                }
            }
            g.spaceAfter = i < text.size();
            groups.push_back (std::move (g));
        }
    }

    // ---- Währungen: $ vor Zahl
    // ---- Aussprache von hinten nach vorne (Kontext: kommt danach ein Vokal?)
    std::vector<std::vector<std::u32string>> phon (groups.size());
    Ctx ctx;
    for (int gi = (int) groups.size() - 1; gi >= 0; --gi)
    {
        auto& g = groups[(size_t) gi];
        auto& out = phon[(size_t) gi];
        out.assign (g.subs.size(), {});

        // Gruppe aus mehreren Wort-Teilen ohne Leerzeichen: erst als Ganzes versuchen (z. B. "e-mail")
        int firstWord = -1, lastWord = -1, wordParts = 0;
        bool onlyWordsAndHyphens = true;
        for (size_t k = 0; k < g.subs.size(); ++k)
        {
            if (g.subs[k].kind == 0) { if (firstWord < 0) firstWord = (int) k; lastWord = (int) k; ++wordParts; }
            else if (! (g.subs[k].kind == 3 && (g.subs[k].text == U"-" || g.subs[k].text == U"'"))) onlyWordsAndHyphens = false;
        }
        if (wordParts > 1 && onlyWordsAndHyphens)
        {
            std::u32string merged;
            for (int k = firstWord; k <= lastWord; ++k) merged += g.subs[(size_t) k].text;
            if (auto r = wordToPhonemes (merged, 0, true, ctx); r.ps && r.rating >= 3)
            {
                out[(size_t) firstWord] = *r.ps;
                for (int k = firstWord + 1; k <= lastWord; ++k) out[(size_t) k] = U"";
                ctx = nextCtx (ctx, *r.ps, merged);
                // Satzzeichen danach/davor trotzdem übernehmen
                for (size_t k = 0; k < g.subs.size(); ++k)
                    if (g.subs[k].kind == 2) out[k] = g.subs[k].text == U"(" ? U"(" : g.subs[k].text == U")" ? U")" : g.subs[k].text;
                continue;
            }
        }

        char32_t currency = 0;
        for (size_t k = 0; k + 1 < g.subs.size(); ++k)
            if (g.subs[k].kind == 3 && (g.subs[k].text == U"$" || g.subs[k].text == U"£" || g.subs[k].text == U"€") && g.subs[k + 1].kind == 1)
                currency = g.subs[k].text[0];

        for (int k = (int) g.subs.size() - 1; k >= 0; --k)
        {
            auto& s = g.subs[(size_t) k];
            std::u32string ps;
            if (s.kind == 2)
                ps = s.text;
            else if (s.kind == 3)
            {
                if (s.text == U"-" || s.text == U"–") ps = (g.subs.size() == 1) ? U"—" : U"";
                else if (s.text == U"%" || s.text == U"&" || s.text == U"+" || s.text == U"@")
                {
                    if (auto r = specialCase (s.text, "", std::nullopt, ctx); r.ps) ps = *r.ps;
                }
                else ps = U"";
            }
            else
            {
                const bool isHead = k == 0 || g.subs[(size_t) k - 1].kind != 1;
                auto r = wordToPhonemes (s.text, s.kind == 1 ? currency : 0, isHead, ctx);
                if (r.ps) ps = *r.ps;
                else if (s.kind == 0) ps = fallback (s.text);
            }
            out[(size_t) k] = ps;
            if (s.kind != 3) ctx = nextCtx (ctx, ps, s.text);
        }

        // Betonung innerhalb einer zusammengesetzten Gruppe ausgleichen (wie misaki resolve_tokens)
        if (wordParts > 1 && onlyWordsAndHyphens)
        {
            std::vector<std::tuple<bool, int, int>> idx;
            for (int k = firstWord; k <= lastWord; ++k)
                if (! out[(size_t) k].empty() && g.subs[(size_t) k].kind == 0)
                    idx.push_back ({ out[(size_t) k].find (PRIMARY) != std::u32string::npos, stressWeight (out[(size_t) k]), k });
            int primaries = 0;
            for (auto& t : idx) primaries += std::get<0> (t) ? 1 : 0;
            if (idx.size() >= 2 && primaries > ((int) idx.size() + 1) / 2)
            {
                std::sort (idx.begin(), idx.end());
                for (size_t q = 0; q < idx.size() / 2; ++q)
                    out[(size_t) std::get<2> (idx[q])] = applyStress (out[(size_t) std::get<2> (idx[q])], -0.5);
            }
        }
    }

    // ---- Zusammensetzen
    std::u32string result;
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        auto& g = groups[gi];
        std::u32string gs;
        bool hyphenGroup = false;
        for (auto& s : g.subs) if (s.kind == 3 && s.text == U"-") hyphenGroup = true;
        for (size_t k = 0; k < g.subs.size(); ++k)
        {
            const auto& p = phon[gi][k];
            if (p.empty()) continue;
            const bool isPunct = g.subs[k].kind == 2;
            if (! gs.empty() && ! isPunct && g.subs[k].text != U"(" && (hyphenGroup || g.subs[k].kind == 1
                                                                        || (k > 0 && g.subs[k - 1].kind != g.subs[k].kind
                                                                            && g.subs[k - 1].kind != 2)))
                gs += U" ";
            else if (! gs.empty() && ! isPunct && g.subs[k].kind == 0 && k > 0 && g.subs[k - 1].kind == 0)
                gs += U" ";
            gs += p;
        }
        if (gs.empty()) continue;
        if (! result.empty() && result.back() != U' ')
        {
            const bool startsPunct = has (NON_QUOTE_PUNCTS, gs.front()) || gs.front() == U')';
            if (! startsPunct) result += U" ";
        }
        result += gs;
    }

    // Kokoro v1.0: ɾ -> T, ʔ -> t
    replaceAll (result, U'ɾ', U"T");
    replaceAll (result, U'ʔ', U"t");
    // doppelte Leerzeichen raus
    std::u32string clean;
    for (auto c : result)
        if (! (c == U' ' && ! clean.empty() && clean.back() == U' ')) clean.push_back (c);
    while (! clean.empty() && clean.back() == U' ') clean.pop_back();
    return toU8 (clean);
}
} // namespace tts
