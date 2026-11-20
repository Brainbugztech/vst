#pragma once

// Eigene Bedienelemente: Wellenform-Radio, Huellkurven-Feld, Filterkurve (Serum-Stil), LFO-Anzeige, Key-Modus

#include <juce_audio_processors/juce_audio_processors.h>
#include <complex>
#include "PlutoLook.h"
#include "Params.h"
#include "Wavetables.h"

namespace pluto
{
// Kleiner Helfer: haengt einen Parameter an und ruft bei Aenderung repaint/Callback
struct ParamLink
{
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    float value = 0.0f;

    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& id, std::function<void()> onChange)
    {
        param = s.getParameter (id);
        jassert (param != nullptr);
        att = std::make_unique<juce::ParameterAttachment> (*param, [this, onChange] (float v) { value = v; if (onChange) onChange(); });
        att->sendInitialUpdate();
    }
    float norm() const { return param->convertTo0to1 (value); }
    void begin() { att->beginGesture(); }
    void setPart (float v) { att->setValueAsPartOfGesture (param->getNormalisableRange().snapToLegalValue (v)); }
    void end() { att->endGesture(); }
    void setOnce (float v) { att->setValueAsCompleteGesture (param->getNormalisableRange().snapToLegalValue (v)); }
};

// ---------------- Wellenform-Radiobuttons ----------------
class WaveSelector : public juce::Component
{
public:
    std::function<void (int)> onWaveChanged;

    WaveSelector (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        link.attach (s, id, [this] { repaint(); if (onWaveChanged) onWaveChanged ((int) link.value); });
    }
    int current() const { return (int) link.value; }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.setFont (PlutoLook::font (11.0f));
        g.setColour (col::text);
        g.drawText ("Wave", 0, 0, getWidth(), 13, Justification::centred);
        const auto& names = waveNames();
        for (int i = 0; i < names.size(); ++i)
        {
            auto row = rowBounds (i);
            g.setColour (col::text);
            g.drawText (names[i], row.withWidth (row.getWidth() - 16), Justification::centredLeft);
            auto c = row.removeFromRight (14).toFloat().reduced (1.0f);
            g.setColour (col::border);
            g.fillEllipse (c);
            if (i == (int) link.value)
            {
                g.setColour (col::cyan);
                g.fillEllipse (c.reduced (2.0f));
            }
            else
            {
                g.setColour (Colour (0xff3a3a3a));
                g.fillEllipse (c.reduced (2.0f));
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < waveNames().size(); ++i)
            if (rowBounds (i).contains (e.getPosition())) { link.setOnce ((float) i); return; }
    }

private:
    ParamLink link;
    juce::Rectangle<int> rowBounds (int i) const
    {
        const float rh = (float) (getHeight() - 14) / (float) waveNames().size();
        return { 0, 14 + (int) (i * rh), getWidth(), (int) rh };
    }
};

// ---------------- Huellkurven-Feld (ADSR mit Griffen) ----------------
class EnvDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    EnvDisplay (juce::AudioProcessorValueTreeState& s, int osc)
    {
        a.attach (s, oid (osc, "a"), [this] { repaint(); });
        d.attach (s, oid (osc, "d"), [this] { repaint(); });
        su.attach (s, oid (osc, "s"), [this] { repaint(); });
        r.attach (s, oid (osc, "r"), [this] { repaint(); });
        setTooltip ("Huellkurve: Griffe ziehen (Attack, Decay/Sustain, Release). Doppelklick = Standard");
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (col::cyanBox);
        g.fillRect (b);
        g.setColour (col::teal.withAlpha (0.18f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (b.getHeight() * i / 4), 0, b.getWidth());

        const auto p = points();
        Path fill, line;
        line.startNewSubPath (p[0]); line.lineTo (p[1]); line.lineTo (p[2]); line.lineTo (p[3]); line.lineTo (p[4]);
        fill = line;
        fill.lineTo (p[4].x, b.getBottom()); fill.lineTo (p[0].x, b.getBottom()); fill.closeSubPath();
        g.setColour (col::teal.withAlpha (0.25f));
        g.fillPath (fill);
        g.setColour (col::textDark);
        g.strokePath (line, PathStrokeType (1.3f));

        for (int i = 1; i <= 4; ++i)
        {
            if (i == 3) continue;
            auto hb = Rectangle<float> (9, 9).withCentre (p[i]);
            g.setColour (i == drag ? col::red : Colour (0xff4a4a4a));
            g.fillRect (hb);
            g.setColour (col::cyan);
            g.fillRect (hb.reduced (2.5f));
        }

        g.setColour (col::textDark.withAlpha (0.8f));
        g.setFont (PlutoLook::font (9.5f));
        if (drag > 0)
        {
            String t = drag == 1 ? "A " + ms (a.value)
                     : drag == 2 ? "D " + ms (d.value) + "  S " + String (su.value, 2)
                     : "R " + ms (r.value);
            g.drawText (t, b.reduced (3, 1), Justification::topRight);
        }
        g.setColour (col::border);
        g.drawRect (b, 1.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = points();
        drag = 0;
        float best = 14.0f;
        for (int i : { 1, 2, 4 })
        {
            const float dd = p[i].getDistanceFrom (e.position);
            if (dd < best) { best = dd; drag = i; }
        }
        if (drag == 0) drag = e.position.x < p[1].x ? 1 : (e.position.x < p[2].x ? 2 : 4);
        if (drag == 1) a.begin(); else if (drag == 2) { d.begin(); su.begin(); } else r.begin();
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float sw = segW();
        auto toMs = [sw] (float px) { const float n = juce::jlimit (0.0f, 1.0f, px / sw); return n * n * 10000.0f; };
        const auto p = points();
        if (drag == 1) a.setPart (toMs (e.position.x - 3.0f));
        else if (drag == 2)
        {
            d.setPart (toMs (e.position.x - p[1].x));
            su.setPart (juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - 3.0f) / ((float) getHeight() - 6.0f)));
        }
        else if (drag == 4) r.setPart (toMs (e.position.x - p[3].x));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag == 1) a.end(); else if (drag == 2) { d.end(); su.end(); } else if (drag == 4) r.end();
        drag = 0;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        for (auto* l : { &a, &d, &su, &r })
            l->setOnce (l->param->convertFrom0to1 (l->param->getDefaultValue()));
    }

private:
    ParamLink a, d, su, r;
    int drag = 0;

    static juce::String ms (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms"; }
    float segW() const { return ((float) getWidth() - 6.0f) / 3.6f; }

    std::array<juce::Point<float>, 5> points() const
    {
        const float sw = segW(), h = (float) getHeight() - 6.0f, top = 3.0f;
        auto w = [sw] (float msv) { return std::sqrt (juce::jlimit (0.0f, 1.0f, msv / 10000.0f)) * sw; };
        const float x0 = 3.0f;
        const float x1 = x0 + w (a.value);
        const float x2 = x1 + w (d.value);
        const float x3 = x2 + sw * 0.6f;
        const float x4 = x3 + w (r.value);
        const float sy = top + (1.0f - su.value) * h;
        return { { { x0, top + h }, { x1, top }, { x2, sy }, { x3, sy }, { x4, top + h } } };
    }
};

// ---------------- Filterkurve im Serum-Stil ----------------
class FilterDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    FilterDisplay (juce::AudioProcessorValueTreeState& s, int f)
    {
        auto rp = [this] { repaint(); };
        type.attach (s, fid (f, "type"), rp);
        cut.attach (s, fid (f, "cut"), rp);
        res.attach (s, fid (f, "res"), rp);
        mix.attach (s, fid (f, "mix"), rp);
        on.attach (s, fid (f, "on"), rp);
        setTooltip ("Punkt ziehen: links/rechts = Cutoff, hoch/runter = Resonanz");
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (col::cyanBox);
        g.fillRect (b);

        // Raster: Oktaven-/Dekadenlinien
        g.setColour (col::teal.withAlpha (0.2f));
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            g.drawVerticalLine ((int) xFor (hz), 0, b.getHeight());
        g.drawHorizontalLine ((int) yFor (0.0f), 0, b.getWidth());
        g.setColour (col::teal.withAlpha (0.1f));
        for (float db : { -24.0f, -12.0f, 12.0f }) g.drawHorizontalLine ((int) yFor (db), 0, b.getWidth());

        // Kurve
        Path curve;
        const int n = juce::jmax (2, getWidth());
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i * b.getWidth() / (float) n;
            const float hz = hzFor (x);
            const float db = Decibels::gainToDecibels (magnitude (hz), -60.0f);
            const float y = jlimit (b.getY(), b.getBottom() + 2, yFor (db));
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        Path fill (curve);
        fill.lineTo (b.getRight(), b.getBottom()); fill.lineTo (b.getX(), b.getBottom()); fill.closeSubPath();
        const float alpha = on.value > 0.5f ? 1.0f : 0.35f;
        g.setGradientFill (ColourGradient (col::teal.withAlpha (0.95f * alpha), 0, 0, col::teal.withAlpha (0.55f * alpha), 0, b.getBottom(), false));
        g.fillPath (fill);
        g.setColour (Colour (0xff163a3a).withAlpha (alpha));
        g.strokePath (curve, PathStrokeType (1.6f));

        // Griff
        auto dot = handlePos();
        g.setColour (Colours::white.withAlpha (0.9f));
        g.fillEllipse (Rectangle<float> (10, 10).withCentre (dot));
        g.setColour (col::textDark);
        g.drawEllipse (Rectangle<float> (10, 10).withCentre (dot), 1.2f);

        // Werte
        g.setFont (PlutoLook::font (9.5f));
        g.setColour (col::textDark.withAlpha (0.75f));
        const float hzv = cut.value;
        String hzs = hzv >= 1000.0f ? String (hzv / 1000.0f, 2) + " kHz" : String (hzv, 1) + " Hz";
        g.drawText (filterTypeNames()[(int) type.value] + "  " + hzs, b.reduced (3, 1), Justification::bottomLeft);

        g.setColour (col::border);
        g.drawRect (b, 1.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override { cut.begin(); res.begin(); mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float x = juce::jlimit (0.0f, (float) getWidth(), e.position.x);
        cut.setPart (hzFor (x));
        res.setPart (juce::jlimit (0.0f, 1.0f, 1.0f - e.position.y / (float) getHeight()));
    }
    void mouseUp (const juce::MouseEvent&) override { cut.end(); res.end(); }

private:
    ParamLink type, cut, res, mix, on;

    float xFor (float hz) const { return std::log (hz / 20.0f) / std::log (1000.0f) * (float) getWidth(); }
    float hzFor (float x) const { return 20.0f * std::pow (1000.0f, x / (float) juce::jmax (1, getWidth())); }
    float yFor (float db) const { const float top = 18.0f, bot = -42.0f; return (top - db) / (top - bot) * (float) getHeight(); }

    juce::Point<float> handlePos() const
    {
        return { xFor (juce::jlimit (20.0f, 20000.0f, cut.value)), (1.0f - res.value) * (float) getHeight() };
    }

    float magnitude (float hz) const
    {
        using C = std::complex<double>;
        const int t = (int) type.value;
        const double fc = cut.value, r = res.value;
        const double sr = 48000.0;
        C h (1.0, 0.0);
        auto stage = [] (double f, double fcv, double k, int kind) -> C
        {
            const C s (0.0, f / fcv);
            const C den = s * s + k * s + 1.0;
            switch (kind)
            {
                case 0: return 1.0 / den;          // LP
                case 1: return s * k / den;        // BP (normiert)
                case 2: return s * s / den;        // HP
                default: return (s * s + 1.0) / den; // Notch
            }
        };
        const double k = 2.0 * (1.0 - r * 0.985);
        switch (t)
        {
            case HP2: h = stage (hz, fc, k, 2); break;
            case HP4: h = stage (hz, fc, k, 2) * stage (hz, fc, 1.414, 2); break;
            case LP2: h = stage (hz, fc, k, 0); break;
            case LP4: h = stage (hz, fc, k, 0) * stage (hz, fc, 1.414, 0); break;
            case BP2: h = stage (hz, fc, k, 1); break;
            case BP4: h = stage (hz, fc, k, 1) * stage (hz, fc, k, 1); break;
            case Notch: h = stage (hz, fc, k, 3); break;
            case CombPlus:
            case CombMinus:
            {
                const double D = sr / std::max (20.0, fc);
                const double fb = (t == CombPlus ? 1.0 : -1.0) * r * 0.97 * 0.85;
                const double w = juce::MathConstants<double>::twoPi * hz / sr;
                h = (1.0 - std::abs (fb) * 0.88) / (1.0 - fb * std::exp (C (0.0, -w * D)));
                break;
            }
            case Formant:
            {
                static const double F[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
                const double pos = juce::jlimit (0.0, 1.0, std::log (std::max (20.0, fc) / 20.0) / std::log (1000.0)) * 4.0;
                const int a = std::min ((int) pos, 3);
                const double fr = pos - a;
                const double kk = 1.0 / (4.0 + r * 18.0);
                const double g3[3] { 1.0, 0.6, 0.35 };
                h = 0.0;
                for (int i = 0; i < 3; ++i) h += stage (hz, F[a][i] * (1 - fr) + F[a + 1][i] * fr, kk, 1) * g3[i] * 3.0;
                break;
            }
            default: break;
        }
        const double m = mix.value;
        if (on.value < 0.5f) return 1.0f;
        return (float) std::abs ((1.0 - m) + m * h);
    }
};

// ---------------- LFO-Anzeige ----------------
class LfoDisplay : public juce::Component
{
public:
    LfoDisplay (juce::AudioProcessorValueTreeState& s, int l) { shape.attach (s, lid (l, "shape"), [this] { repaint(); }); }
    float phase = 0.0f;

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (col::cyanBox);
        g.fillRect (b);
        g.setColour (col::teal.withAlpha (0.25f));
        g.drawHorizontalLine ((int) b.getCentreY(), 0, b.getWidth());
        Path p;
        const int sh = (int) shape.value;
        const int n = 120;
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / n;
            const float v = shapeVal (sh, x);
            const float px = 3 + x * (b.getWidth() - 6), py = b.getCentreY() - v * (b.getHeight() * 0.5f - 4);
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (col::textDark);
        g.strokePath (p, PathStrokeType (1.3f));
        const float px = 3 + phase * (b.getWidth() - 6);
        const float py = b.getCentreY() - shapeVal (sh, phase) * (b.getHeight() * 0.5f - 4);
        g.setColour (col::red);
        g.fillEllipse (px - 3, py - 3, 6, 6);
        g.setColour (col::border);
        g.drawRect (b, 1.0f);
    }

private:
    ParamLink shape;
    static float shapeVal (int s, float x)
    {
        switch (s)
        {
            case 0: return std::sin (juce::MathConstants<float>::twoPi * x);
            case 1: return x < 0.25f ? 4 * x : (x < 0.75f ? 2 - 4 * x : 4 * x - 4);
            case 2: return 2 * x - 1;
            case 3: return 1 - 2 * x;
            case 4: return x < 0.5f ? 1.0f : -1.0f;
            case 5: { static const float v[8] { 0.3f, -0.7f, 0.9f, -0.2f, 0.6f, -0.9f, 0.1f, -0.4f }; return v[juce::jmin (7, (int) (x * 8))]; }
            default: { static const float v[9] { 0.3f, -0.7f, 0.9f, -0.2f, 0.6f, -0.9f, 0.1f, -0.4f, 0.3f };
                       const float p = x * 8; const int i = juce::jmin (7, (int) p); const float t = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (p - i));
                       return v[i] + (v[i + 1] - v[i]) * t; }
        }
    }
};

// ---------------- Key-Modus (Key / Fix / Half) mit -/+ ----------------
class KeyModeSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    KeyModeSelector (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        link.attach (s, id, [this] { repaint(); });
        setTooltip ("Key = folgt der Tastatur, Fix = feste Tonhoehe, Half = halbe Tastaturverfolgung");
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().reduced (0, 1);
        auto lb = r.removeFromLeft (34);
        g.setColour (col::combo); g.fillRect (lb);
        g.setColour (col::border); g.drawRect (lb);
        g.setColour (col::text);
        g.setFont (PlutoLook::font (11.0f));
        g.drawText (keyModeNames()[(int) link.value], lb.reduced (3, 0), Justification::centredLeft);
        r.removeFromLeft (2);
        for (int i = 0; i < 2; ++i)
        {
            auto b = r.removeFromLeft (14);
            r.removeFromLeft (1);
            g.setColour (Colour (0xff6c6c6c)); g.fillRect (b);
            g.setColour (col::border); g.drawRect (b);
            g.setColour (col::text);
            const auto c = b.toFloat().getCentre();
            g.fillRect (c.x - 3.5f, c.y - 0.75f, 7.0f, 1.5f);
            if (i == 1) g.fillRect (c.x - 0.75f, c.y - 3.5f, 1.5f, 7.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int x = e.x;
        if (x < 36) step (1);
        else if (x < 51) step (-1);
        else step (1);
    }

private:
    ParamLink link;
    void step (int d)
    {
        const int n = keyModeNames().size();
        link.setOnce ((float) (((int) link.value + d + n) % n));
    }
};
} // namespace pluto
