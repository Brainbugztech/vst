#include "LabScene.h"

namespace
{
    // Szenen-Geometrie (Canvas-Einheiten 1980 x 880, wie im Prototyp)
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

    const juce::Colour TOX { 182, 255, 31 }, PNK { 255, 47, 142 }, WATER { 60, 170, 180 }, AMBER { 255, 170, 40 };

    juce::Colour mix (juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t)); }
    float frac (double v) { return (float) (v - std::floor (v)); }
}

//==============================================================================
// Ziehbarer ms-Wert auf dem rechten Monitor
class LabScene::MsValue : public juce::Component
{
public:
    MsValue (HaasProcessor& p, int s) : proc (p), side (s)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        setTitle (side == 0 ? "Delay links" : "Delay rechts");
    }

    float current() const
    {
        float l, r;
        bb::HaasEngine::delaysMs (proc.readParams(), l, r);
        return side == 0 ? l : r;
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = side == 0 ? TOX : PNK;
        g.setFont (bb::fonts::mono ((float) getHeight() * 0.92f));
        g.setColour (c.withAlpha (0.35f));
        g.drawText (juce::String (current(), 1), getLocalBounds().translated (0, 1), juce::Justification::centredRight);
        g.setColour (c);
        g.drawText (juce::String (current(), 1), getLocalBounds(), juce::Justification::centredRight);
        if (hasKeyboardFocus (false))
        {
            g.setColour (c.withAlpha (0.6f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1), 3, 1);
        }
    }

    void mouseDown (const juce::MouseEvent&) override { startValue = current(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float sens = e.mods.isShiftDown() ? 0.02f : 0.1f;
        proc.setDisplayedDelay (side, startValue - (float) e.getDistanceFromDragStartY() * sens);
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        step (w.deltaY > 0 ? 1.0f : -1.0f, e.mods.isShiftDown());
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        const bool flip = proc.readParams().flip;
        const bool isLeftBase = (side == 0) != flip;
        proc.setParam (isLeftBase ? ParamIDs::lms : ParamIDs::rms, isLeftBase ? 6.0f : 30.0f);
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.isKeyCode (juce::KeyPress::upKey))   { step (1.0f, k.getModifiers().isShiftDown()); return true; }
        if (k.isKeyCode (juce::KeyPress::downKey)) { step (-1.0f, k.getModifiers().isShiftDown()); return true; }
        return false;
    }
    void step (float dir, bool fine)
    {
        proc.setDisplayedDelay (side, current() + dir * (fine ? 0.1f : 0.5f));
        repaint();
    }

private:
    HaasProcessor& proc;
    int side;
    float startValue = 0.0f;
};

//==============================================================================
LabScene::LabScene (HaasProcessor& p) : proc (p)
{
    setOpaque (true);
    labImg   = juce::ImageCache::getFromMemory (BBAssets::haas_lab_png, BBAssets::haas_lab_pngSize);
    frontImg = juce::ImageCache::getFromMemory (BBAssets::haas_front_png, BBAssets::haas_front_pngSize);
    parts[0] = juce::ImageCache::getFromMemory (BBAssets::body_png, BBAssets::body_pngSize);
    parts[1] = juce::ImageCache::getFromMemory (BBAssets::head_png, BBAssets::head_pngSize);
    parts[2] = juce::ImageCache::getFromMemory (BBAssets::legL_png, BBAssets::legL_pngSize);
    parts[3] = juce::ImageCache::getFromMemory (BBAssets::legR_png, BBAssets::legR_pngSize);

    for (auto& s : btnSeeds) s = rng.nextFloat();

    for (int side = 0; side < 2; ++side)
    {
        msVal[side] = std::make_unique<MsValue> (proc, side);
        addAndMakeVisible (*msVal[side]);
        for (auto* b : { &msDown[side], &msUp[side] })
        {
            b->getProperties().set ("mono", true);
            b->getProperties().set ("accent", side == 0 ? "tox" : "brain");
            addAndMakeVisible (*b);
        }
        msDown[side].setButtonText (juce::String::fromUTF8 ("\xe2\x88\x92"));
        msUp[side].setButtonText ("+");
        msDown[side].setTitle (side == 0 ? "L kuerzer" : "R kuerzer");
        msUp[side].setTitle (side == 0 ? "L laenger" : "R laenger");
        msDown[side].onClick = [this, side] { msVal[side]->step (-1.0f, juce::ModifierKeys::currentModifiers.isShiftDown()); };
        msUp[side].onClick   = [this, side] { msVal[side]->step (1.0f, juce::ModifierKeys::currentModifiers.isShiftDown()); };
    }

    lastMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

LabScene::~LabScene() { stopTimer(); }

void LabScene::resized()
{
    const float s = (float) getWidth() / CW;
    // Panel auf dem rechten Monitor (78.2 % / 24 % wie im Prototyp)
    const float px = 1548.0f, rowH = 36.0f;
    for (int side = 0; side < 2; ++side)
    {
        const float y = 211.0f + side * 46.0f;
        auto R = [s] (float x, float yy, float w, float h)
        {
            return juce::Rectangle<float> (x * s, yy * s, w * s, h * s).toNearestInt();
        };
        msDown[side].setBounds (R (px + 40, y, 40, rowH));
        msVal[side]->setBounds (R (px + 86, y - 2, 108, rowH + 4));
        msUp[side].setBounds (R (px + 202, y, 40, rowH));
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

    const float fN = prm.mono ? 0.0f : std::min (1.0f, prm.width) + std::max (0.0f, prm.width - 1.0f) * 0.9f;
    smoothF += (fN - smoothF) * 0.12f;

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

    // Glitch-Bursts auf Achteln
    const float gl = prm.mono ? 0.0f : smoothF;
    if (act)
    {
        const auto bt = (long long) std::floor (beats * 2.0);
        if (bt != glitchBeat)
        {
            glitchBeat = bt;
            if (rng.nextFloat() < 0.35f + gl * 0.55f) burst = 1.0f;
        }
    }
    else if (rng.nextFloat() < 0.004f + gl * 0.01f)
        burst = 1.0f;
    burst = std::max (0.0f, burst - (float) dt * 5.0f);

    // Blasen
    const float heat = std::min (1.0f, 0.35f + 0.4f * std::min (1.0f, prm.width) * 0.6f + 0.6f * env);
    if (bubbles.size() < 26 && rng.nextFloat() < 0.35f + heat * 0.4f)
    {
        const bool fromBug = rng.nextFloat() < 0.4f;
        Bubble b;
        if (fromBug) { b.x = TX0 + 200.0f + rng.nextFloat() * 460.0f; b.y = 470.0f; b.r = 2.0f + rng.nextFloat() * 5.0f; }
        else         { b.x = TX0 + 40.0f + rng.nextFloat() * (TX1 - TX0 - 80.0f); b.y = GLB - 6.0f; b.r = 2.0f + rng.nextFloat() * 8.0f; }
        b.s = 40.0f + rng.nextFloat() * 60.0f;
        b.w = rng.nextFloat() * 6.0f;
        bubbles.push_back (b);
    }
    for (auto it = bubbles.begin(); it != bubbles.end();)
    {
        it->y -= it->s * (float) dt * (1.0f + heat);
        it->x += std::sin ((float) tt * 3.0f + it->w) * 0.4f;
        if (it->y < WTOP + 4.0f) it = bubbles.erase (it);
        else ++it;
    }

    msVal[0]->repaint();
    msVal[1]->repaint();
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

void LabScene::renderBugBuffer (float phys)
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
    part (1, 0.0f, headPivot, 0.0f);
}

//==============================================================================
void LabScene::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / CW;
    const float phys = s * g.getInternalContext().getPhysicalPixelScaleFactor();
    ensureCaches (phys);

    const auto prm = proc.readParams();
    const bool playing = proc.vizPlaying.load();
    const float env = proc.vizEnv.load();
    const bool act = playing || env > 0.002f;
    const float heat = std::min (1.0f, 0.35f + 0.24f * std::min (1.0f, prm.width) + 0.6f * env);
    const auto mixC = mix (TOX, PNK, std::min (1.0f, smoothF));

    g.addTransform (juce::AffineTransform::scale (s));
    g.drawImage (labCache, juce::Rectangle<float> (0, 0, CW, CH));

    drawConsoles (g, act, env);

    renderBugBuffer (phys);
    drawWater (g, act, env, heat, mixC);

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
        if (k == 0 && ledOn) { g.setColour (mixC.withAlpha (0.3f)); g.fillEllipse (x - 12, y - 12, 24, 24); }
        g.setColour (c);
        g.fillEllipse (x - 5, y - 5, 10, 10);
    }

    // Lichtschein des Tanks auf dem Tisch
    juce::ColourGradient spill (WATER.withAlpha (0.14f), (TX0 + TX1) / 2, GROUND + 20, WATER.withAlpha (0.0f),
                                (TX0 + TX1) / 2 + 620, GROUND + 20, true);
    g.setGradientFill (spill);
    g.fillRect (0.0f, GROUND, CW, CH - GROUND);
}

//==============================================================================
void LabScene::drawWater (juce::Graphics& g, bool act, float env, float heat, juce::Colour mixC)
{
    const auto prm = proc.readParams();
    const float t = (float) tt;
    const float bob = std::sin (t * 0.8f) * 12.0f, sway = std::sin (t * 0.45f) * 16.0f, tilt = std::sin (t * 0.6f) * 0.018f;
    const float bugCX = (TX0 + TX1) / 2 + sway, bugCY = 480.0f + bob;
    const float X0 = bugCX - (SCX - BOX) * K, Y0 = bugCY - (SCY - BOY) * K;
    auto toCanvas = [&] (float sx, float sy)
    {
        const float dx = (sx - SCX) * K, dy = (sy - SCY) * K;
        return juce::Point<float> (bugCX + dx * std::cos (tilt) - dy * std::sin (tilt),
                                   bugCY + dx * std::sin (tilt) + dy * std::cos (tilt));
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
        g.setGradientFill (juce::ColourGradient (WATER.withAlpha (0.10f), 0, GLT, WATER.withAlpha (0.0f), 0, GLB, false));
        g.fillPath (ray);
    }

    // Kabel mit fließender Nährflüssigkeit
    const float feed = act ? std::max (0.0f, 1.0f - frac (proc.vizBeats.load()) * 3.0f) : 0.0f;
    for (int ci = 0; ci < 3; ++ci)
    {
        const auto& c = cables[ci];
        const auto p3 = toCanvas (c.sx, c.sy);
        juce::Path p;
        p.startNewSubPath (c.top, GLT - 4);
        p.cubicTo ({ c.top, GLT + 150 }, { p3.x + (ci - 1) * 30.0f, p3.y - 170 }, p3);
        g.setColour (juce::Colour (0xff0c0a10));
        g.strokePath (p, juce::PathStrokeType (18, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (150, 230, 190).withAlpha (0.14f));
        g.strokePath (p, juce::PathStrokeType (9, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        flowDash (g, p, 30, 46, (float) flowPos + ci * 23.0f, 10, juce::Colour (205, 255, 225).withAlpha (0.12f));
        flowDash (g, p, 30, 46, (float) flowPos + ci * 23.0f, 6, juce::Colour (205, 255, 225).withAlpha (0.42f));
        juce::ColourGradient fg (juce::Colour (205, 255, 225).withAlpha (0.18f + 0.25f * feed), p3.x, p3.y,
                                 juce::Colour (205, 255, 225).withAlpha (0.0f), p3.x + 34, p3.y, true);
        g.setGradientFill (fg);
        g.fillEllipse (p3.x - 34, p3.y - 34, 68, 68);
    }

    // Käfer mit Lichtbrechung und Video-Glitch
    {
        juce::Graphics::ScopedSaveState bugState (g);
        g.addTransform (juce::AffineTransform::rotation (tilt, bugCX, bugCY));
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

        const float gl = (prm.mono) ? 0.0f : smoothF;
        const float gs = gl * (0.35f + 0.65f * burst) + (act ? env * gl * 0.4f : 0.0f);

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
            const int blocks = juce::roundToInt (std::min (gs, 2.0f) * 14.0f);
            for (int k = 0; k < blocks; ++k)
            {
                const float bw = 8 + rng.nextFloat() * 30, bh = 6 + rng.nextFloat() * 16;
                const float bx = BW * 0.15f + rng.nextFloat() * BW * 0.7f, by = BH * 0.1f + rng.nextFloat() * BH * 0.7f;
                g.drawImage (bugBuf, juce::roundToInt (X0 + bx + (rng.nextFloat() - 0.5f) * 30 * gs), juce::roundToInt (Y0 + by),
                             (int) bw, (int) bh, juce::roundToInt (bx * py), juce::roundToInt (by * py),
                             std::max (1, juce::roundToInt (bw * py)), std::max (1, juce::roundToInt (bh * py)));
            }
            if (burst > 0.5f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                for (float y = Y0; y < Y0 + BH; y += 3) g.fillRect (X0, y, (float) BW, 1.0f);
            }
        }
    }

    // Gehirn-Glühen im Wasser
    const auto gp2 = toCanvas (830, 450);
    const float fbA = std::min (1.0f, prm.width) * 0.6f;
    g.setGradientFill (juce::ColourGradient (mixC.withAlpha (0.10f + 0.22f * fbA + 0.3f * env), gp2.x, gp2.y,
                                             mixC.withAlpha (0.0f), gp2.x + 380, gp2.y, true));
    g.fillRect (TX0, GLT, TX1 - TX0, GLB - GLT);

    // Wasserkörper
    juce::Path wp;
    wp.startNewSubPath (TX0, GLB);
    auto surfY = [t] (float x) { return WTOP + std::sin (x * 0.018f + t * 1.8f) * 3.0f + std::sin (x * 0.047f - t * 2.7f) * 1.6f; };
    for (float x = TX0; x <= TX1; x += 20) wp.lineTo (x, surfY (x));
    wp.lineTo (TX1, GLB);
    wp.closeSubPath();
    juce::ColourGradient wt (WATER.withAlpha (0.16f), 0, WTOP, juce::Colour (8, 40, 52).withAlpha (0.55f), 0, GLB, false);
    wt.addColour (0.6, juce::Colour (30, 110, 125).withAlpha (0.24f));
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
    juce::ignoreUnused (heat);
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
    const float sx = x + 46, sy = CTOP + 22, sw = CONW - 92, sh = 190;
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
    const float corr = act ? proc.vizCorr.load() : 1.0f;

    if (ch == 0)
    {
        // Vektorskop: Mitte vertikal, Seite horizontal
        const float cxm = sx + sw / 2, cym = sy + sh / 2 + 8;
        g.setColour (colr.withAlpha (0.18f));
        g.drawLine (cxm - 80, cym - 80, cxm + 80, cym + 80);
        g.drawLine (cxm + 80, cym - 80, cxm - 80, cym + 80);
        g.setColour (colr.withAlpha (0.75f));
        if (act)
        {
            const float gain = std::min (260.0f, 120.0f / std::max (0.05f, proc.vizEnv.load()));
            for (int k = 0; k < HaasProcessor::kScopeSize; ++k)
            {
                const float l = proc.scope[(size_t) k * 2].load (std::memory_order_relaxed);
                const float r = proc.scope[(size_t) k * 2 + 1].load (std::memory_order_relaxed);
                const float X = cxm + (r - l) * gain * 0.7f, Y = cym - (l + r) * gain * 0.5f;
                if (Y > sy + 30 && Y < sy + sh - 26) g.fillRect (X - 1.5f, Y - 1.5f, 3.0f, 3.0f);
            }
        }
        else
            for (int k = 0; k < 60; ++k)
            {
                const float v = std::sin (t * 2 + k * 0.3f) * 30;
                g.fillRect (cxm + std::sin ((float) k) * prm.width * 10 - 1, cym - v - 1, 3.0f, 3.0f);
            }
        g.setColour (colr.withAlpha (0.85f));
        g.drawText ("VEKTORSKOP", juce::Rectangle<float> (sx + 12, sy + 8, sw - 24, 20), juce::Justification::left);
        g.drawText ("KORR " + juce::String (corr >= 0 ? "+" : "") + juce::String (corr, 2),
                    juce::Rectangle<float> (sx + 12, sy + sh - 28, sw - 24, 20), juce::Justification::left);
    }
    else
    {
        g.setColour (colr.withAlpha (0.85f));
        g.drawText ("HAAS DELAY", juce::Rectangle<float> (sx + 12, sy + 8, sw - 24, 20), juce::Justification::left);
        // Kanal-Buchstaben und Einheit neben den Bedienelementen
        g.setFont (bb::fonts::mono (28));
        g.setColour (TOX);
        g.drawText ("L", juce::Rectangle<float> (sx + 14, 211, 30, 36), juce::Justification::centredLeft);
        g.setColour (PNK);
        g.drawText ("R", juce::Rectangle<float> (sx + 14, 257, 30, 36), juce::Justification::centredLeft);
        g.setFont (bb::fonts::mono (17));
        g.setColour (TOX.withAlpha (0.75f));
        g.drawText ("ms", juce::Rectangle<float> (sx + 308, 211, 50, 36), juce::Justification::centredLeft);
        g.setColour (PNK.withAlpha (0.75f));
        g.drawText ("ms", juce::Rectangle<float> (sx + 308, 257, 50, 36), juce::Justification::centredLeft);

        const float bx0 = sx + 24, bw = sw - 48, byy = sy + 140;
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (bx0, byy, bw, 10.0f);
        const float cxp = bx0 + bw * (corr + 1) / 2;
        g.setColour (corr < 0 ? juce::Colour (255, 80, 60) : TOX);
        g.fillRect (std::min (cxp, bx0 + bw / 2), byy, std::abs (cxp - (bx0 + bw / 2)), 10.0f);
        g.setFont (bb::fonts::mono (15));
        g.setColour (colr.withAlpha (0.7f));
        g.drawText ("-1", juce::Rectangle<float> (bx0, byy + 14, 40, 20), juce::Justification::left);
        g.drawText ("+1", juce::Rectangle<float> (bx0 + bw - 40, byy + 14, 40, 20), juce::Justification::right);
        g.drawText (prm.mono ? "MONO-CHECK" : "STEREO", juce::Rectangle<float> (bx0, byy + 14, bw, 20), juce::Justification::centred);
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
