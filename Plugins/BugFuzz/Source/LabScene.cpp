#include "LabScene.h"

namespace
{
    // Szenen-Geometrie (Canvas-Einheiten 1980 x 880, wie beim Haas)
    constexpr float GROUND = 792.0f;
    constexpr float TX0 = 560.0f, TX1 = 1420.0f, LID0 = 34.0f, GLT = 86.0f, GLB = GROUND - 26.0f, WTOP = 150.0f;
    constexpr float K = 0.86f, SCX = 826.0f, SCY = 550.0f, BOX = 308.0f, BOY = 272.0f;
    constexpr float CONW = 460.0f, CTOP = 150.0f;
    constexpr int   BW = 892, BH = 480; // Käfer-Puffer in Canvas-Einheiten

    struct PartRect { float x, y, w, h; };
    // Reihenfolge: body, head, legL, legR (Szenen-Koordinaten der zerlegten Logo-Ebenen)
    constexpr PartRect partRects[4] { { 430, 475, 859, 237 }, { 562, 322, 559, 279 }, { 358, 460, 302, 288 }, { 961, 478, 334, 302 } };
    const juce::Point<float> pivotL { 607, 525 }, pivotR { 997, 540 }, headPivot { 836, 596 };

    struct Cable { float top, sx, sy; };
    constexpr Cable cables[3] { { 905, 848, 362 }, { 1005, 928, 378 }, { 1120, 1058, 404 } };

    // Bildschirme der Monitore (wie im Hintergrundbild)
    constexpr float SCR_Y = CTOP + 22.0f, SCR_W = CONW - 92.0f, SCR_H = 190.0f;
    constexpr float RSCR_X = 1980.0f - 40.0f - CONW + 46.0f;           // 1526
    const juce::Rectangle<float> panelArea { RSCR_X + 8.0f, SCR_Y + 30.0f, SCR_W - 16.0f, SCR_H - 36.0f };

    const juce::Colour TOX { 182, 255, 31 }, PNK { 255, 47, 142 }, WATER { 60, 170, 180 }, AMBER { 255, 170, 40 };

    juce::Colour mix (juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t)); }
    float frac (double v) { return (float) (v - std::floor (v)); }
    juce::Colour bandColour (int b) { return mix (TOX, PNK, (float) b / 3.0f); }

    juce::String hzText (float v)
    {
        if (v >= 10000.0f) return juce::String (v / 1000.0f, 1) + "k";
        if (v >= 1000.0f)  return juce::String (v / 1000.0f, 2) + "k";
        return juce::String (juce::roundToInt (v));
    }
}

//==============================================================================
// Vier Band-Säulen (Drive je Band) und drei Weichen-Frequenzen, alles ziehbar
class LabScene::BandPanel : public juce::Component
{
public:
    explicit BandPanel (BugFuzzProcessor& p) : proc (p)
    {
        setWantsKeyboardFocus (true);
        setTitle ("Baender und Weichen");
    }

    // Layout in Canvas-Einheiten (Breite panelArea.getWidth())
    static constexpr float colW = 88.0f, barTop = 4.0f, barH = 96.0f, labelY = 104.0f, freqY = 126.0f;

    float s() const { return (float) getWidth() / panelArea.getWidth(); }

    void paint (juce::Graphics& g) override
    {
        const float sc = s();
        g.addTransform (juce::AffineTransform::scale (sc));
        const auto prm = proc.readParams();
        const auto x = bb::BugFuzzEngine::sanitiseXover (prm.xover);
        const char* names[4] { "LO", "LM", "HM", "HI" };

        for (int b = 0; b < 4; ++b)
        {
            const float cx = colW * (b + 0.5f);
            const auto colr = bandColour (b);
            const float v = prm.band[(size_t) b];
            const float lvl = std::min (1.0f, proc.vizBand[(size_t) b].load() * 3.0f);
            const bool hot = hover == b || dragBand == b || (focusItem == b && hasKeyboardFocus (false));

            // Säule aus 16 Segmenten
            const int segs = 16;
            const float segH = barH / segs;
            const float bx = cx - 20.0f, bw = 40.0f;
            for (int k = 0; k < segs; ++k)
            {
                const float y = barTop + barH - (k + 1) * segH;
                const bool on = (k + 0.5f) / segs <= v;
                const float a = on ? 0.55f + 0.45f * lvl : 0.08f;
                g.setColour (colr.withAlpha (a));
                g.fillRect (bx, y + 1.0f, bw, segH - 2.0f);
            }
            if (lvl > 0.02f)
            {
                g.setGradientFill (juce::ColourGradient (colr.withAlpha (0.25f * lvl), cx, barTop + barH * (1 - v),
                                                         colr.withAlpha (0.0f), cx + 44, barTop + barH * (1 - v), true));
                g.fillEllipse (cx - 44, barTop + barH * (1 - v) - 44, 88, 88);
            }
            if (hot)
            {
                g.setColour (colr.withAlpha (0.7f));
                g.drawRect (bx - 4, barTop - 2, bw + 8, barH + 4, 1.0f);
            }

            g.setFont (bb::fonts::mono (15.0f));
            g.setColour (colr);
            g.drawText (juce::String (names[b]) + " " + juce::String (juce::roundToInt (v * 100.0f)),
                        juce::Rectangle<float> (cx - colW / 2, labelY, colW, 18), juce::Justification::centred);
        }

        // Weichen
        for (int i = 0; i < 3; ++i)
        {
            const float cx = colW * (i + 1);
            const bool hot = hoverX == i || dragX == i || (focusItem == 4 + i && hasKeyboardFocus (false));
            const auto colr = mix (bandColour (i), bandColour (i + 1), 0.5f);
            g.setColour (colr.withAlpha (0.25f));
            g.drawVerticalLine ((int) cx, barTop, barTop + barH);
            g.setFont (bb::fonts::mono (14.0f));
            g.setColour (hot ? colr : colr.withAlpha (0.75f));
            g.drawText (hzText (x[(size_t) i]), juce::Rectangle<float> (cx - 40, freqY, 80, 18), juce::Justification::centred);
            if (hot)
            {
                g.setColour (colr.withAlpha (0.6f));
                g.drawRoundedRectangle (cx - 32, freqY - 1, 64, 20, 3, 1);
            }
        }
    }

    // Trefferprüfung: 0..3 = Band, 4..6 = Weiche, -1 = nichts
    int itemAt (juce::Point<float> local) const
    {
        const auto p = local / s();
        if (p.y >= freqY - 4)
        {
            const int i = juce::roundToInt (p.x / colW) - 1;
            return i >= 0 && i < 3 ? 4 + i : -1;
        }
        const int b = (int) (p.x / colW);
        return b >= 0 && b < 4 ? b : -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int it = itemAt (e.position);
        hover  = it >= 0 && it < 4 ? it : -1;
        hoverX = it >= 4 ? it - 4 : -1;
        setMouseCursor (it >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
    void mouseExit (const juce::MouseEvent&) override { hover = hoverX = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int it = itemAt (e.position);
        if (it < 0) return;
        focusItem = it;
        const auto prm = proc.readParams();
        if (it < 4)
        {
            dragBand = it;
            // Klick in die Säule setzt den Wert direkt
            const float y = e.position.y / s();
            if (y >= barTop && y <= barTop + barH)
                setBand (it, 1.0f - (y - barTop) / barH);
            startValue = proc.readParams().band[(size_t) it];
        }
        else
        {
            dragX = it - 4;
            startValue = bb::BugFuzzEngine::sanitiseXover (prm.xover)[(size_t) dragX];
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float dy = (float) e.getDistanceFromDragStartY() / s();
        const bool fine = e.mods.isShiftDown();
        if (dragBand >= 0)
            setBand (dragBand, startValue - dy / barH * (fine ? 0.25f : 1.0f));
        else if (dragX >= 0)
            setX (dragX, startValue * std::pow (2.0f, -dy * (fine ? 0.004f : 0.02f)));
    }

    void mouseUp (const juce::MouseEvent&) override { dragBand = dragX = -1; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int it = itemAt (e.position);
        if (it >= 0 && it < 4) setBand (it, BugFuzzProcessor::kBandDefaults[it]);
        else if (it >= 4)      setX (it - 4, BugFuzzProcessor::kXoverDefaults[it - 4]);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int it = itemAt (e.position);
        if (it < 0) return;
        step (it, w.deltaY > 0 ? 1.0f : -1.0f, e.mods.isShiftDown());
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        const bool fine = k.getModifiers().isShiftDown();
        if (k.isKeyCode (juce::KeyPress::upKey))    { step (focusItem, 1.0f, fine); return true; }
        if (k.isKeyCode (juce::KeyPress::downKey))  { step (focusItem, -1.0f, fine); return true; }
        if (k.isKeyCode (juce::KeyPress::rightKey)) { focusItem = (focusItem + 1) % 7; repaint(); return true; }
        if (k.isKeyCode (juce::KeyPress::leftKey))  { focusItem = (focusItem + 6) % 7; repaint(); return true; }
        return false;
    }

private:
    void step (int it, float dir, bool fine)
    {
        const auto prm = proc.readParams();
        if (it >= 0 && it < 4) setBand (it, prm.band[(size_t) it] + dir * (fine ? 0.01f : 0.05f));
        else if (it >= 4)      setX (it - 4, bb::BugFuzzEngine::sanitiseXover (prm.xover)[(size_t) (it - 4)]
                                              * std::pow (2.0f, dir * (fine ? 1.0f / 24.0f : 1.0f / 6.0f)));
    }
    void setBand (int b, float v) { proc.setParam (ParamIDs::band[b], juce::jlimit (0.0f, 1.0f, v)); repaint(); }
    void setX (int i, float hz)
    {
        // Nachbarn berücksichtigen, damit die Weichen in Reihenfolge bleiben
        const auto x = bb::BugFuzzEngine::sanitiseXover (proc.readParams().xover);
        float lo = bb::BugFuzzEngine::kXMin[i], hi = bb::BugFuzzEngine::kXMax[i];
        if (i > 0) lo = std::max (lo, x[(size_t) i - 1] * 1.5f);
        if (i < 2) hi = std::min (hi, x[(size_t) i + 1] / 1.5f);
        proc.setParam (ParamIDs::xover[i], juce::jlimit (lo, std::max (lo, hi), hz));
        repaint();
    }

    BugFuzzProcessor& proc;
    int hover = -1, hoverX = -1, dragBand = -1, dragX = -1, focusItem = 1;
    float startValue = 0.0f;
};

//==============================================================================
LabScene::LabScene (BugFuzzProcessor& p) : proc (p)
{
    setOpaque (true);
    labImg   = juce::ImageCache::getFromMemory (BBAssets::haas_lab_png, BBAssets::haas_lab_pngSize);
    frontImg = juce::ImageCache::getFromMemory (BBAssets::haas_front_png, BBAssets::haas_front_pngSize);
    parts[0] = juce::ImageCache::getFromMemory (BBAssets::body_png, BBAssets::body_pngSize);
    parts[1] = juce::ImageCache::getFromMemory (BBAssets::head_png, BBAssets::head_pngSize);
    parts[2] = juce::ImageCache::getFromMemory (BBAssets::legL_png, BBAssets::legL_pngSize);
    parts[3] = juce::ImageCache::getFromMemory (BBAssets::legR_png, BBAssets::legR_pngSize);

    for (auto& s : btnSeeds) s = rng.nextFloat();
    buildFur();

    bandPanel = std::make_unique<BandPanel> (proc);
    addAndMakeVisible (*bandPanel);

    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

LabScene::~LabScene() { stopTimer(); }

void LabScene::resized()
{
    const float s = (float) getWidth() / CW;
    bandPanel->setBounds (juce::Rectangle<float> (panelArea.getX() * s, panelArea.getY() * s,
                                                  panelArea.getWidth() * s, panelArea.getHeight() * s).toNearestInt());
}

//==============================================================================
// Haarwurzeln entlang der Oberkante des Gehirns finden
void LabScene::buildFur()
{
    const auto& img = parts[1];
    if (! img.isValid()) return;
    constexpr int cell = 4;
    const int gw = img.getWidth() / cell, gh = img.getHeight() / cell;
    std::vector<float> m ((size_t) (gw * gh), 0.0f);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int gy = 0; gy < gh; ++gy)
            for (int gx = 0; gx < gw; ++gx)
            {
                float sum = 0.0f;
                for (int y = 0; y < cell; ++y)
                    for (int x = 0; x < cell; ++x)
                        sum += bd.getPixelColour (gx * cell + x, gy * cell + y).getFloatAlpha();
                m[(size_t) (gy * gw + gx)] = sum / (cell * cell);
            }
    }
    auto on = [&] (int x, int y) { return x >= 0 && y >= 0 && x < gw && y < gh && m[(size_t) (y * gw + x)] > 0.5f; };

    juce::Random r (4711);
    std::vector<juce::Point<float>> taken;
    for (int gy = 0; gy < gh; ++gy)
        for (int gx = 0; gx < gw; ++gx)
        {
            if (! on (gx, gy)) continue;
            juce::Point<float> n;
            int off = 0;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    if ((dx || dy) && ! on (gx + dx, gy + dy)) { n += { (float) dx, (float) dy }; ++off; }
            if (off == 0 || n.getDistanceFromOrigin() < 0.5f) continue;
            n /= n.getDistanceFromOrigin();
            if (n.y > 0.3f) continue; // nur Oberseite und Flanken
            const juce::Point<float> pos (partRects[1].x + (gx + 0.5f) * cell, partRects[1].y + (gy + 0.5f) * cell);
            bool close = false;
            for (auto& t : taken) if (t.getDistanceFrom (pos) < 9.0f) { close = true; break; }
            if (close) continue;
            taken.push_back (pos);
            Hair h;
            h.p = pos - n * 5.0f; // Wurzel etwas nach innen, damit sie unter dem Gehirn verschwindet
            h.ang = std::atan2 (n.y, n.x);
            h.seed = r.nextFloat();
            h.len = 0.6f + 0.8f * r.nextFloat();
            fur.push_back (h);
        }
}

//==============================================================================
void LabScene::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    dt = juce::jlimit (0.0, 0.05, (now - lastMs) / 1000.0);
    lastMs = now;
    tt += dt;

    const auto prm = proc.readParams();
    const bool playing = proc.vizPlaying.load();
    const float env = proc.vizEnv.load();
    const bool act = playing || env > 0.002f;
    const double beats = proc.vizBeats.load();
    const double bpm = proc.vizBpm.load();
    const float krass = std::max (0.0f, prm.drive - 0.5f) * 2.0f;

    smoothF += (prm.drive - smoothF) * 0.12f;
    const float furT = prm.drive * (0.75f + (act ? std::min (1.0f, env * 3.0f) * 0.5f : 0.0f));
    furUp += (furT - furUp) * (furT > furUp ? 0.35f : 0.06f);

    // Lage des Käfers
    const float t = (float) tt;
    bugCX = (TX0 + TX1) / 2 + std::sin (t * 0.45f) * 16.0f;
    bugCY = 480.0f + std::sin (t * 0.8f) * 12.0f;
    bugTilt = std::sin (t * 0.6f) * 0.018f;

    // Paddel-Zyklus: ein Schlag pro Beat, sonst langsames Treiben
    const double strokeHz = act ? bpm / 60.0 : 0.4;
    if (act)
    {
        double d = beats - lastBeats;
        if (d < 0.0 || d > 1.0) d = strokeHz * dt;
        gp += d * juce::MathConstants<double>::twoPi;
        lastBeats = beats;
    }
    else
        gp += strokeHz * dt * juce::MathConstants<double>::twoPi;

    flowPos += dt * (act ? bpm / 60.0 * 70.0 : 22.0);

    // Glitch-Bursts auf Achteln, erst im Krass-Bereich richtig heftig
    bool eighth = false;
    if (act)
    {
        const auto bt = (long long) std::floor (beats * 2.0);
        if (bt != glitchBeat)
        {
            glitchBeat = bt;
            eighth = true;
            if (rng.nextFloat() < 0.15f + krass * 0.6f) burst = 1.0f;
        }
    }
    else if (rng.nextFloat() < 0.002f + krass * 0.01f)
        burst = 1.0f;
    burst = std::max (0.0f, burst - (float) dt * 5.0f);

    // Funken von den Kabeln ins Gehirn
    auto toCanvas = [this] (juce::Point<float> sp)
    {
        const float dx = (sp.x - SCX) * K, dy = (sp.y - SCY) * K;
        return juce::Point<float> (bugCX + dx * std::cos (bugTilt) - dy * std::sin (bugTilt),
                                   bugCY + dx * std::sin (bugTilt) + dy * std::cos (bugTilt));
    };
    float pArc = prm.drive > 0.12f ? prm.drive * 0.05f + (act ? env * prm.drive * 0.8f : 0.0f) : 0.0f;
    int spawn = rng.nextFloat() < std::min (0.6f, pArc) ? 1 : 0;
    if (eighth && prm.drive > 0.3f) spawn += 1 + (krass > 0.5f ? 1 : 0);
    for (int k = 0; k < spawn && ! fur.empty() && arcs.size() < 10; ++k)
    {
        const auto& c = cables[rng.nextInt (3)];
        const auto a = toCanvas ({ c.sx, c.sy });
        const auto& h = fur[(size_t) rng.nextInt ((int) fur.size())];
        const auto b = toCanvas (h.p + juce::Point<float> (std::cos (h.ang), std::sin (h.ang)) * 12.0f);
        Arc arc;
        const int segs = 7;
        const auto d = b - a;
        const juce::Point<float> nrm (-d.y, d.x);
        const float nl = std::max (1.0f, nrm.getDistanceFromOrigin());
        for (int i = 0; i <= segs; ++i)
        {
            const float f = (float) i / segs;
            const float j = (i == 0 || i == segs) ? 0.0f : (rng.nextFloat() - 0.5f) * (26.0f + 30.0f * krass);
            arc.pts.push_back (a + d * f + nrm / nl * j);
        }
        arc.life = 1.0f;
        const float pick = rng.nextFloat();
        arc.c = pick < 0.4f ? TOX : (pick < 0.8f ? PNK : juce::Colour (230, 240, 255));
        arcs.push_back (std::move (arc));
    }
    for (auto it = arcs.begin(); it != arcs.end();)
    {
        it->life -= (float) dt * 9.0f;
        if (it->life <= 0.0f) it = arcs.erase (it);
        else ++it;
    }

    // Blasen: mehr und schneller, je heißer der Fuzz
    const float heat = std::min (1.0f, 0.3f + 0.5f * prm.drive + 0.6f * env);
    if (bubbles.size() < (size_t) (20 + 20 * krass) && rng.nextFloat() < 0.3f + heat * 0.5f)
    {
        const bool fromBug = rng.nextFloat() < 0.45f;
        Bubble b;
        if (fromBug) { b.x = TX0 + 200.0f + rng.nextFloat() * 460.0f; b.y = 440.0f; b.r = 2.0f + rng.nextFloat() * 5.0f; }
        else         { b.x = TX0 + 40.0f + rng.nextFloat() * (TX1 - TX0 - 80.0f); b.y = GLB - 6.0f; b.r = 2.0f + rng.nextFloat() * 8.0f; }
        b.s = 40.0f + rng.nextFloat() * 60.0f;
        b.w = rng.nextFloat() * 6.0f;
        bubbles.push_back (b);
    }
    for (auto it = bubbles.begin(); it != bubbles.end();)
    {
        it->y -= it->s * (float) dt * (1.0f + heat);
        it->x += std::sin ((float) tt * 3.0f + it->w) * (0.4f + krass * 0.8f);
        if (it->y < WTOP + 4.0f) it = bubbles.erase (it);
        else ++it;
    }

    bandPanel->repaint();
    repaint();
}

//==============================================================================
void LabScene::ensureCaches (float phys)
{
    if (std::abs (phys - cacheScale) < 0.001f && labCache.isValid())
        return;
    cacheScale = phys;
    const int w = juce::roundToInt (CW * phys), h = juce::roundToInt (CH * phys);
    labCache   = labImg.rescaled (w, h, juce::Graphics::highResamplingQuality);
    frontCache = frontImg.rescaled (w, h, juce::Graphics::highResamplingQuality);
    const float f = K * phys;
    for (int i = 0; i < 4; ++i)
        partsScaled[i] = parts[i].rescaled (std::max (1, juce::roundToInt (partRects[i].w * f)),
                                            std::max (1, juce::roundToInt (partRects[i].h * f)),
                                            juce::Graphics::highResamplingQuality);
    bugBuf = juce::Image (juce::Image::ARGB, std::max (1, juce::roundToInt (BW * phys)),
                          std::max (1, juce::roundToInt (BH * phys)), true);
}

void LabScene::renderBugBuffer (float phys, float env)
{
    bugBuf.clear (bugBuf.getBounds());
    juce::Graphics bg (bugBuf);
    bg.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    const float f = K * phys;
    const float amp = 9.0f * juce::MathConstants<float>::pi / 180.0f;
    const float sw = std::sin ((float) gp), sw2 = std::sin ((float) gp + 0.7f);

    auto part = [&] (int idx, float rot, juce::Point<float> pv, float dy)
    {
        const auto& r = partRects[idx];
        auto t = juce::AffineTransform::translation ((r.x - pv.x) * f, (r.y - pv.y) * f)
                     .rotated (rot)
                     .translated ((pv.x - BOX) * f, (pv.y - BOY + dy) * f);
        bg.drawImageTransformed (partsScaled[idx], t);
    };

    part (0, 0.0f, headPivot, 0.0f);
    part (2, sw * amp, pivotL, -std::max (0.0f, std::cos ((float) gp)) * 6.0f);
    part (3, -sw2 * amp, pivotR, -std::max (0.0f, std::cos ((float) gp + 0.7f)) * 6.0f);

    // Fell hinter dem Gehirn: stellt sich mit dem Drive auf, wird im Krass-Bereich struppig
    if (furUp > 0.01f)
    {
        const float t = (float) tt;
        const float krass = std::max (0.0f, smoothF - 0.5f) * 2.0f;
        const float pulse = std::min (1.0f, env * 3.0f);
        juce::Random jr ((juce::int64) (tt * 30.0)); // Zittern, ändert sich mit der Bildrate
        for (const auto& h : fur)
        {
            const float L = h.len * (3.0f + 34.0f * furUp) * (1.0f + 0.25f * pulse);
            float a = h.ang + std::sin (t * (2.0f + h.seed * 3.0f) + h.seed * 20.0f) * (0.12f + 0.3f * krass);
            if (krass > 0.0f) a += (jr.nextFloat() - 0.5f) * 0.5f * krass;
            const juce::Point<float> dir (std::cos (a), std::sin (a));
            const juce::Point<float> side (-dir.y, dir.x);
            const auto base = (h.p - juce::Point<float> (BOX, BOY)) * f;
            const auto tip  = base + dir * (L + 5.0f) * f;
            const auto ctrl = base + dir * (L * 0.5f) * f + side * std::sin (t * 1.7f + h.seed * 9.0f) * L * 0.22f * f;
            juce::Path p;
            p.startNewSubPath (base);
            p.quadraticTo (ctrl, tip);
            bg.setColour (juce::Colour (24, 14, 34).withAlpha (0.95f));
            bg.strokePath (p, juce::PathStrokeType (2.8f * f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const auto tipC = mix (PNK, TOX, h.seed).withAlpha (0.55f + 0.4f * pulse);
            juce::Path tp;
            tp.startNewSubPath (ctrl + (tip - ctrl) * 0.35f);
            tp.lineTo (tip);
            bg.setColour (tipC);
            bg.strokePath (tp, juce::PathStrokeType (1.8f * f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    part (1, 0.0f, headPivot, 0.0f);
}

//==============================================================================
void LabScene::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / CW;
    const float phys = s * g.getInternalContext().getPhysicalPixelScaleFactor();
    ensureCaches (phys);

    const bool playing = proc.vizPlaying.load();
    const float env = proc.vizEnv.load();
    const bool act = playing || env > 0.002f;
    const auto mixC = mix (TOX, PNK, smoothF);

    g.addTransform (juce::AffineTransform::scale (s));
    g.drawImage (labCache, juce::Rectangle<float> (0, 0, CW, CH));
    drawPlates (g);

    drawConsoles (g, act, env);

    renderBugBuffer (phys, env);
    drawWater (g, act, env, mixC);

    g.drawImage (frontCache, juce::Rectangle<float> (0, 0, CW, CH));
    drawHoses (g, act);

    // LEDs am Deckel, blinken im Takt
    const bool ledOn = act && frac (proc.vizBeats.load()) < 0.18f;
    for (int k = 0; k < 4; ++k)
    {
        const float x = TX1 - 40.0f - k * 26.0f, y = LID0 + 26.0f;
        juce::Colour c = juce::Colour (80, 70, 90).withAlpha (0.8f);
        if (k == 0 && ledOn) c = mixC;
        else if (k == 1 && act) c = TOX.withAlpha (0.8f);
        else if (k == 2 && smoothF > 0.5f) c = PNK.withAlpha (0.5f + 0.5f * (smoothF - 0.5f) * 2.0f);
        if (k == 0 && ledOn) { g.setColour (mixC.withAlpha (0.3f)); g.fillEllipse (x - 12, y - 12, 24, 24); }
        g.setColour (c);
        g.fillEllipse (x - 5, y - 5, 10, 10);
    }

    // Lichtschein des Tanks auf dem Tisch
    const auto spillC = mix (WATER, PNK, std::max (0.0f, smoothF - 0.5f));
    juce::ColourGradient spill (spillC.withAlpha (0.14f), (TX0 + TX1) / 2, GROUND + 20, spillC.withAlpha (0.0f),
                                (TX0 + TX1) / 2 + 620, GROUND + 20, true);
    g.setGradientFill (spill);
    g.fillRect (0.0f, GROUND, CW, CH - GROUND);
}

// Neue Beschriftung auf den Typenschildern unter den Monitoren
void LabScene::drawPlates (juce::Graphics& g)
{
    const juce::Rectangle<float> plates[2] { { 200, 372, 140, 18 }, { 1640, 372, 140, 18 } };
    const char* text[2] { "BB-LAB  FUZZ", "BB-LAB  BANDS" };
    for (int i = 0; i < 2; ++i)
    {
        g.setColour (juce::Colour (12, 11, 14));
        g.fillRect (plates[i].reduced (2, 1));
        g.setFont (bb::fonts::mono (12.5f));
        g.setColour (juce::Colour (0xff8a8094));
        g.drawText (text[i], plates[i], juce::Justification::centred);
    }
}

//==============================================================================
void LabScene::drawWater (juce::Graphics& g, bool act, float env, juce::Colour mixC)
{
    const auto prm = proc.readParams();
    const float t = (float) tt;
    const float krass = std::max (0.0f, smoothF - 0.5f) * 2.0f;
    const auto waterC = mix (WATER, juce::Colour (120, 70, 170), krass * 0.5f);
    const float X0 = bugCX - (SCX - BOX) * K, Y0 = bugCY - (SCY - BOY) * K;
    auto toCanvas = [&] (float sx, float sy)
    {
        const float dx = (sx - SCX) * K, dy = (sy - SCY) * K;
        return juce::Point<float> (bugCX + dx * std::cos (bugTilt) - dy * std::sin (bugTilt),
                                   bugCY + dx * std::sin (bugTilt) + dy * std::cos (bugTilt));
    };

    juce::Graphics::ScopedSaveState tank (g);
    g.reduceClipRegion (juce::Rectangle<float> (TX0, GLT, TX1 - TX0, GLB - GLT).toNearestInt());

    // Lichtstrahlen von oben
    for (int k = 0; k < 6; ++k)
    {
        const float x = TX0 + 90 + k * 140 + std::sin (t * 0.35f + k * 1.7f) * 40;
        juce::Path ray;
        ray.startNewSubPath (x, GLT);
        ray.lineTo (x + 60, GLT);
        ray.lineTo (x + 150, GLB);
        ray.lineTo (x + 40, GLB);
        ray.closeSubPath();
        g.setGradientFill (juce::ColourGradient (waterC.withAlpha (0.10f), 0, GLT, waterC.withAlpha (0.0f), 0, GLB, false));
        g.fillPath (ray);
    }

    // Kabel: hier fließt Strom statt Nährflüssigkeit
    const float feed = act ? std::max (0.0f, 1.0f - frac (proc.vizBeats.load()) * 3.0f) : 0.0f;
    const auto juice = mix (juce::Colour (205, 255, 225), TOX, prm.drive);
    for (int ci = 0; ci < 3; ++ci)
    {
        const auto& c = cables[ci];
        const auto p3 = toCanvas (c.sx, c.sy);
        juce::Path p;
        p.startNewSubPath (c.top, GLT - 4);
        p.cubicTo ({ c.top, GLT + 150 }, { p3.x + (ci - 1) * 30.0f, p3.y - 170 }, p3);
        g.setColour (juce::Colour (0xff0c0a10));
        g.strokePath (p, juce::PathStrokeType (18, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juice.withAlpha (0.14f));
        g.strokePath (p, juce::PathStrokeType (9, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float sp = 1.0f + prm.drive * 2.0f;
        flowDash (g, p, 18, 30, (float) flowPos * sp + ci * 23.0f, 10, juice.withAlpha (0.12f));
        flowDash (g, p, 18, 30, (float) flowPos * sp + ci * 23.0f, 6, juice.withAlpha (0.5f));
        juce::ColourGradient fg (juice.withAlpha (0.18f + 0.3f * feed * prm.drive + 0.2f * env), p3.x, p3.y,
                                 juice.withAlpha (0.0f), p3.x + 40, p3.y, true);
        g.setGradientFill (fg);
        g.fillEllipse (p3.x - 40, p3.y - 40, 80, 80);
    }

    // Käfer mit Lichtbrechung und Video-Glitch
    {
        juce::Graphics::ScopedSaveState bugState (g);
        g.addTransform (juce::AffineTransform::rotation (bugTilt, bugCX, bugCY));
        const int bwp = bugBuf.getWidth(), bhp = bugBuf.getHeight();
        const float py = (float) bhp / (float) BH; // Puffer-Pixel pro Canvas-Einheit

        auto refr = [&] (float X, juce::Colour tint, bool silhouette)
        {
            for (int y = 0; y < BH; y += 6)
            {
                const float dx = std::sin ((Y0 + y) * 0.03f + t * 2.2f) * 3.2f;
                const int sy = juce::roundToInt (y * py), sh = std::max (1, juce::roundToInt (6 * py));
                if (silhouette) g.setColour (tint);
                else g.setOpacity (1.0f);
                g.drawImage (bugBuf, juce::roundToInt (X + dx), juce::roundToInt (Y0 + y), BW, 6,
                             0, sy, bwp, std::min (sh, bhp - sy), silhouette);
            }
        };

        const float gs = krass * (0.25f + 0.75f * burst) + (act ? env * krass * 0.5f : 0.0f) + burst * 0.15f;

        if (gs > 0.02f)
        {
            const float ca = 3.0f + gs * 16.0f;
            const float a = 0.32f * std::min (1.0f, gs * 1.4f);
            refr (X0 - ca, juce::Colour (255, 40, 90).withAlpha (a), true);
            refr (X0 + ca, juce::Colour (40, 230, 255).withAlpha (a), true);
        }
        refr (X0, {}, false);

        if (gs > 0.05f)
        {
            const int n = juce::roundToInt (2.0f + std::min (gs, 2.0f) * 9.0f);
            for (int k = 0; k < n; ++k)
            {
                const int sy = rng.nextInt (BH);
                const int sh = std::min (4 + rng.nextInt (juce::jmax (1, (int) (10 + gs * 40))), BH - sy);
                const float dx = (rng.nextFloat() - 0.5f) * gs * 120.0f;
                g.setOpacity (1.0f);
                g.drawImage (bugBuf, juce::roundToInt (X0 + dx), juce::roundToInt (Y0 + sy), BW, sh,
                             0, juce::roundToInt (sy * py), bwp, std::max (1, juce::roundToInt (sh * py)));
                if (rng.nextFloat() < 0.4f)
                {
                    g.setColour ((rng.nextBool() ? TOX : PNK).withAlpha (0.25f * std::min (1.0f, gs)));
                    g.fillRect (X0 + dx + rng.nextFloat() * BW * 0.6f, Y0 + sy, 40 + rng.nextFloat() * 160, (float) std::min (sh, 6));
                }
            }
            if (burst > 0.5f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                for (float y = Y0; y < Y0 + BH; y += 3) g.fillRect (X0, y, (float) BW, 1.0f);
            }
        }
    }

    drawArcs (g);

    // Gehirn-Glühen im Wasser
    const auto gp2 = toCanvas (830, 450);
    g.setGradientFill (juce::ColourGradient (mixC.withAlpha (0.10f + 0.2f * prm.drive + 0.3f * env), gp2.x, gp2.y,
                                             mixC.withAlpha (0.0f), gp2.x + 380, gp2.y, true));
    g.fillRect (TX0, GLT, TX1 - TX0, GLB - GLT);

    // Wasserkörper, kocht im Krass-Bereich leicht
    juce::Path wp;
    wp.startNewSubPath (TX0, GLB);
    const float boil = 1.0f + krass * 2.5f;
    auto surfY = [t, boil] (float x) { return WTOP + std::sin (x * 0.018f + t * 1.8f * boil) * 3.0f * boil + std::sin (x * 0.047f - t * 2.7f * boil) * 1.6f * boil; };
    for (float x = TX0; x <= TX1; x += 20) wp.lineTo (x, surfY (x));
    wp.lineTo (TX1, GLB);
    wp.closeSubPath();
    juce::ColourGradient wt (waterC.withAlpha (0.16f), 0, WTOP, juce::Colour (8, 40, 52).withAlpha (0.55f), 0, GLB, false);
    wt.addColour (0.6, waterC.withMultipliedBrightness (0.7f).withAlpha (0.24f));
    g.setGradientFill (wt);
    g.fillPath (wp);

    // Kaustik-Schimmer
    {
        juce::Graphics::ScopedSaveState cs (g);
        g.reduceClipRegion (wp);
        const float span = TX1 - TX0 - 120;
        for (int k = 0; k < 7; ++k)
        {
            float x = std::fmod (k * 173.0f + t * 22.0f * (k % 2 ? 1.0f : -1.0f), span);
            if (x < 0) x += span;
            x += TX0 + 60;
            const float y = WTOP + 60 + (float) ((k * 97) % 380) + std::sin (t + k) * 20;
            g.setGradientFill (juce::ColourGradient (juce::Colour (160, 240, 255).withAlpha (0.06f), x, y,
                                                     juce::Colour (160, 240, 255).withAlpha (0.0f), x + 90, y, true));
            g.fillEllipse (x - 90, y - 90, 180, 180);
        }
    }

    // Blasen
    for (const auto& b : bubbles)
    {
        g.setColour (juce::Colour (210, 245, 255).withAlpha (0.08f));
        g.fillEllipse (b.x - b.r, b.y - b.r, b.r * 2, b.r * 2);
        g.setColour (juce::Colour (210, 245, 255).withAlpha (0.45f));
        g.drawEllipse (b.x - b.r, b.y - b.r, b.r * 2, b.r * 2, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        const float hr = b.r * 0.28f;
        g.fillEllipse (b.x - b.r * 0.35f - hr, b.y - b.r * 0.35f - hr, hr * 2, hr * 2);
    }

    // Wasseroberfläche
    juce::Path surf;
    for (float x = TX0; x <= TX1; x += 20)
        x == TX0 ? surf.startNewSubPath (x, surfY (x)) : surf.lineTo (x, surfY (x));
    g.setColour (juce::Colour (200, 245, 255).withAlpha (0.45f));
    g.strokePath (surf, juce::PathStrokeType (2.5f));
}

void LabScene::drawArcs (juce::Graphics& g)
{
    for (const auto& a : arcs)
    {
        juce::Path p;
        p.startNewSubPath (a.pts.front());
        for (size_t i = 1; i < a.pts.size(); ++i) p.lineTo (a.pts[i]);
        g.setColour (a.c.withAlpha (0.18f * a.life));
        g.strokePath (p, juce::PathStrokeType (14.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        g.setColour (a.c.withAlpha (0.8f * a.life));
        g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.9f * a.life));
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        const auto e = a.pts.back();
        g.setGradientFill (juce::ColourGradient (a.c.withAlpha (0.45f * a.life), e.x, e.y, a.c.withAlpha (0.0f), e.x + 30, e.y, true));
        g.fillEllipse (e.x - 30, e.y - 30, 60, 60);
    }
}

//==============================================================================
void LabScene::drawConsoles (juce::Graphics& g, bool act, float env)
{
    const double beats = proc.vizBeats.load();
    const float t = (float) tt;

    for (int ch = 0; ch < 2; ++ch)
    {
        const float x = ch == 0 ? 40.0f : CW - 40.0f - CONW;
        drawScreen (g, x, ch, act);

        // Tastenreihen auf dem Pult
        const float by = 470.0f;
        for (int r = 0; r < 2; ++r)
            for (int k = 0; k < 10; ++k)
            {
                const float bx2 = x + 30 + k * 40 + r * 10, by2 = by - 50 + r * 26;
                const float seed = btnSeeds[(k + r * 10 + ch * 7) % 32];
                const bool lit = act ? (((long long) std::floor (beats * 2) + k * 3 + r * 5 + ch) % 7 < 2 + juce::roundToInt (seed * 2))
                                     : (std::sin (t * 0.7f + seed * 9) > 0.6f);
                const auto cc = k % 3 == 0 ? PNK : (k % 3 == 1 ? TOX : AMBER);
                if (lit)
                {
                    g.setColour (cc.withAlpha (0.25f));
                    g.fillRoundedRectangle (bx2 - 5, by2 - 5, 38, 26, 7);
                }
                g.setColour (lit ? cc.withAlpha (0.95f) : cc.withMultipliedBrightness (0.22f));
                g.fillRoundedRectangle (bx2, by2, 28, 16, 4);
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillRect (bx2 + 3, by2 + 2, 22.0f, 2.0f);
            }

        // VU-Nadel
        const float lvl = act ? std::min (1.0f, env * 1.8f) : 0.05f + 0.03f * std::sin (t * 3 + ch);
        const float a = juce::MathConstants<float>::pi * (1.2f + lvl * 0.6f);
        const float vx = x + CONW / 2, vy = by + 128;
        g.setColour (juce::Colour (0xff1a1512));
        g.drawLine (vx, vy, vx + std::cos (a) * 74, vy + std::sin (a) * 74, 2.0f);

        // Betriebs-LED
        if (act)
        {
            g.setColour (TOX.withAlpha (0.3f));
            g.fillEllipse (x + CONW - 52, by + 28, 24, 24);
        }
        g.setColour (act ? TOX : juce::Colour (80, 110, 40));
        g.fillEllipse (x + CONW - 46, by + 34, 12, 12);
    }
}

void LabScene::drawScreen (juce::Graphics& g, float x, int ch, bool act)
{
    const auto prm = proc.readParams();
    const float sx = x + 46, sy = SCR_Y, sw = SCR_W, sh = SCR_H;
    const auto colr = ch ? PNK : TOX;
    const float t = (float) tt;

    juce::Graphics::ScopedSaveState st (g);
    juce::Path clip;
    clip.addRoundedRectangle (sx, sy, sw, sh, 10);
    g.reduceClipRegion (clip);

    g.setGradientFill (juce::ColourGradient (colr.withMultipliedBrightness (0.12f), sx + sw / 2, sy + sh / 2,
                                             juce::Colour (0xff010302), sx + sw / 2 + sw * 0.7f, sy + sh / 2, true));
    g.fillRect (sx, sy, sw, sh);
    g.setColour (colr.withAlpha (0.10f));
    for (float gx = sx + sw / 8; gx < sx + sw; gx += sw / 8) g.drawVerticalLine ((int) gx, sy, sy + sh);
    for (float gy = sy + sh / 4; gy < sy + sh; gy += sh / 4) g.drawHorizontalLine ((int) gy, sx, sx + sw);

    g.setFont (bb::fonts::mono (15));

    if (ch == 0)
    {
        // Oszilloskop: Ausgangssignal, auf die Bildschirmhöhe normiert
        const float cym = sy + sh / 2 + 6;
        if (act)
        {
            const int w = proc.scopeWrite.load (std::memory_order_relaxed);
            const int nPts = 256;
            const float gain = std::min (70.0f, 34.0f / std::max (0.05f, proc.vizEnv.load()));
            juce::Path sc;
            for (int k = 0; k < nPts; ++k)
            {
                const int idx = (w - nPts + k + BugFuzzProcessor::kScopeSize) % BugFuzzProcessor::kScopeSize;
                const float v = juce::jlimit (-62.0f, 62.0f, proc.scope[(size_t) idx].load (std::memory_order_relaxed) * gain);
                const float X = sx + 8 + (sw - 16) * k / (nPts - 1.0f);
                k == 0 ? sc.startNewSubPath (X, cym - v) : sc.lineTo (X, cym - v);
            }
            g.setColour (PNK.withAlpha (0.18f));
            g.strokePath (sc, juce::PathStrokeType (5.0f));
            g.setColour (PNK.withAlpha (0.55f));
            g.strokePath (sc, juce::PathStrokeType (1.6f));
        }

        // Fuzz-Kennlinie: Eingang (x) gegen Ausgang (y) mit dem aktuellen Drive
        const float cxm = sx + sw / 2, rx = sw * 0.34f, ry = sh * 0.3f;
        g.setColour (colr.withAlpha (0.22f));
        g.drawLine (cxm - rx, cym, cxm + rx, cym);
        g.drawLine (cxm, cym - ry - 10, cxm, cym + ry + 10);
        const float gainLin = std::pow (10.0f, bb::BugFuzzEngine::bandGainDb (prm.drive, std::max (0.3f, prm.band[1])) / 20.0f);
        const bb::BugFuzzEngine::Shaper shaper (prm.drive);
        juce::Path curve;
        for (int k = 0; k <= 120; ++k)
        {
            const float xin = -1.0f + 2.0f * k / 120.0f;
            const float y = shaper (xin * 0.35f * gainLin) / 1.35f;
            const float X = cxm + xin * rx, Y = cym - y * ry;
            k == 0 ? curve.startNewSubPath (X, Y) : curve.lineTo (X, Y);
        }
        const float wob = act ? std::min (1.0f, proc.vizEnv.load() * 3.0f) : 0.3f + 0.2f * std::sin (t * 2.0f);
        g.setColour (colr.withAlpha (0.2f + 0.15f * wob));
        g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colr.withAlpha (0.9f));
        g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (colr.withAlpha (0.85f));
        g.drawText ("FUZZ-KURVE", juce::Rectangle<float> (sx + 12, sy + 8, sw - 24, 20), juce::Justification::left);
        g.setColour ((prm.drive > 0.5f ? PNK : colr).withAlpha (0.85f));
        g.drawText (prm.drive > 0.5f ? "KRASS" : "FUZZ", juce::Rectangle<float> (sx + 12, sy + 8, sw - 24, 20), juce::Justification::right);
        g.setColour (colr.withAlpha (0.7f));
        g.drawText ("DRIVE " + juce::String (juce::roundToInt (prm.drive * 100.0f)) + "%",
                    juce::Rectangle<float> (sx + 12, sy + sh - 28, sw - 24, 20), juce::Justification::left);
        g.drawText (prm.octa ? "OCTA AN" : "OCTA AUS", juce::Rectangle<float> (sx + 12, sy + sh - 28, sw - 24, 20), juce::Justification::right);
    }
    else
    {
        g.setColour (colr.withAlpha (0.85f));
        g.drawText (juce::String::fromUTF8 ("4 B\xc3\x84NDER"), juce::Rectangle<float> (sx + 12, sy + 8, sw - 24, 20), juce::Justification::left);
        g.setColour (colr.withAlpha (0.55f));
        g.drawText ("ZIEHEN", juce::Rectangle<float> (sx + 12, sy + 8, sw - 24, 20), juce::Justification::right);
        // Säulen und Frequenzen zeichnet das BandPanel darüber
    }

    // Scanlines und Glasreflex
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (float y = sy; y < sy + sh; y += 4) g.fillRect (sx, y, sw, 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), sx, sy,
                                             juce::Colours::white.withAlpha (0.0f), sx + sw * 0.25f, sy + sh * 0.4f, false));
    g.fillRect (sx, sy, sw, sh);
}

//==============================================================================
void LabScene::flowDash (juce::Graphics& g, const juce::Path& path, float dash, float gap, float offset,
                         float width, juce::Colour c)
{
    const float period = dash + gap;
    juce::Path out;
    juce::PathFlatteningIterator it (path, {}, 0.5f);
    float dist = 0.0f;
    bool on = false;
    while (it.next())
    {
        const juce::Point<float> a (it.x1, it.y1), b (it.x2, it.y2);
        const float len = a.getDistanceFrom (b);
        if (len <= 0.0f) continue;
        const int steps = std::max (1, (int) std::ceil (len / 3.0f));
        for (int i = 0; i <= steps; ++i)
        {
            const float d = dist + len * (float) i / (float) steps;
            float ph = std::fmod (d - offset, period);
            if (ph < 0) ph += period;
            const bool nowOn = ph < dash;
            const auto p = a + (b - a) * ((float) i / (float) steps);
            if (nowOn && ! on) out.startNewSubPath (p);
            else if (nowOn) out.lineTo (p);
            on = nowOn;
        }
        dist += len;
    }
    g.setColour (c);
    g.strokePath (out, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LabScene::hose (juce::Graphics& g, juce::Point<float> p0, juce::Point<float> c1, juce::Point<float> c2,
                     juce::Point<float> p1, float flow)
{
    juce::Path p;
    p.startNewSubPath (p0);
    p.cubicTo (c1, c2, p1);
    g.setColour (juce::Colour (0xff09080b));
    g.strokePath (p, juce::PathStrokeType (34, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff2a2530));
    g.strokePath (p, juce::PathStrokeType (28, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    flowDash (g, p, 5, 7, flow * 40.0f, 28, juce::Colour (0xff121016));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.strokePath (p, juce::PathStrokeType (4, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (0, -8));
}

void LabScene::drawHoses (juce::Graphics& g, bool act)
{
    const float flow = act ? (float) proc.vizBeats.load() : (float) tt * 0.3f;
    const float W = CW;
    hose (g, { 40 + CONW - 30, 400 }, { 40 + CONW + 60, 330 }, { TX0 - 170, 600 }, { TX0 - 46, 600 }, flow);
    hose (g, { 40 + CONW - 60, GROUND - 40 }, { 40 + CONW + 40, GROUND - 10 }, { TX0 + 20, GROUND + 10 }, { TX0 + 60, GLB + 8 }, flow * 0.8f);
    hose (g, { W - 40 - CONW + 30, 400 }, { W - 40 - CONW - 60, 330 }, { TX1 + 170, 600 }, { TX1 + 46, 600 }, flow);
    hose (g, { W - 40 - CONW + 60, GROUND - 40 }, { W - 40 - CONW - 40, GROUND - 10 }, { TX1 - 20, GROUND + 10 }, { TX1 - 60, GLB + 8 }, flow * 0.8f);
}
