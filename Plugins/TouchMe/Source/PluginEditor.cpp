#include "PluginEditor.h"

using namespace swarm;

namespace
{
    // Design-Koordinaten aus 05_SPEZIFIKATIONEN/layout_original_pixel_exact.json
    const juce::Rectangle<float> rPad    { 348.0f, 214.0f, 604.0f, 499.0f };
    const juce::Rectangle<float> rHold   { 38.0f, 833.0f, 244.0f, 75.0f };
    const juce::Rectangle<float> rHoldLed{ 233.0f, 854.0f, 20.0f, 22.0f };
    const juce::Rectangle<float> rPrev   { 1027.0f, 592.0f, 158.0f, 69.0f };
    const juce::Rectangle<float> rNext   { 1189.0f, 592.0f, 170.0f, 69.0f };
    const juce::Rectangle<float> rSave   { 1370.0f, 592.0f, 136.0f, 69.0f };
    const juce::Rectangle<float> rTopL   { 1034.0f, 50.0f, 61.0f, 68.0f };
    const juce::Rectangle<float> rTopR   { 1436.0f, 50.0f, 48.0f, 68.0f };
    const juce::Rectangle<float> rCat    { 1028.0f, 236.0f, 476.0f, 54.0f };
    const juce::Rectangle<float> rRows   { 1039.0f, 313.0f, 412.0f, 31.4f * 8.0f };
    const juce::Rectangle<float> rScrUp  { 1460.0f, 299.0f, 38.0f, 43.0f };
    const juce::Rectangle<float> rScrDn  { 1460.0f, 533.0f, 38.0f, 44.0f };
    const juce::Rectangle<float> rScrTrk { 1460.0f, 343.0f, 38.0f, 190.0f };
    const juce::Rectangle<float> rTrack  { 478.0f, 916.0f, 375.0f, 40.0f };
    const juce::Rectangle<float> rSlider { 470.0f, 900.0f, 390.0f, 66.0f };
    const juce::Rectangle<float> rWave   { 1038.0f, 748.0f, 462.0f, 178.0f };

    constexpr float fxX = 35.0f, fxY0 = 254.0f, fxW = 255.0f, fxH = 33.0f, fxPitch = 35.0f;
    constexpr float rowPitch = 31.4f;
    constexpr int visibleRows = 8;
    constexpr float thumbC0 = 503.0f, thumbSpan = 316.0f; // Mitte des Dry/Wet-Reglers bei 0 % / Weg bis 100 %

    const juce::Colour cText   { 0xffd2d4d7 };
    const juce::Colour cLabel  { 0xffcbced2 };
    const juce::Colour cCyan   { 0xff8ae6f5 };
    const juce::Colour cCyanHi { 0xffa8f0fa };
    const juce::Colour cActive { 0xff74e9f3 };
    const juce::Colour cHl     { 0xff23bed6 };
    const juce::Colour cHlText { 0xff031414 };
    const juce::Colour cSep    { 0xff0f3744 };
    const juce::Colour cWave   { 0xff59edf8 };

    juce::Rectangle<float> fxRect (int i) { return { fxX, fxY0 + (float) i * fxPitch, fxW, fxH }; }

    juce::Image loadImg (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

    float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    // Text mit weichem Leuchten wie auf den Original-LCDs
    // weight: zusätzliche Strichstärke in Pixeln (die Original-Schrift ist kräftiger als Share Tech Mono)
    void glowText (juce::Graphics& g, const juce::String& s, const juce::Font& f, juce::Colour c, float x, float baseline, float glow, float weight = 0.0f)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, x, baseline);
        juce::Path p;
        ga.createPath (p);
        if (glow > 0.0f)
        {
            g.setColour (c.withAlpha (0.10f * glow));
            g.strokePath (p, juce::PathStrokeType (weight + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (c.withAlpha (0.16f * glow));
            g.strokePath (p, juce::PathStrokeType (weight + 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.fillPath (p);
        if (weight > 0.0f) g.strokePath (p, juce::PathStrokeType (weight, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
TouchMeLook::TouchMeLook()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff031c27));
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cHl);
    setColour (juce::PopupMenu::highlightedTextColourId, cHlText);
    setColour (juce::PopupMenu::headerTextColourId, cCyan);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff2b3540));
    setColour (juce::AlertWindow::textColourId, cText);
    setColour (juce::AlertWindow::outlineColourId, juce::Colour (0xff131820));
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff031c27));
    setColour (juce::TextEditor::textColourId, cCyan);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff0d6178));
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff24d9f2));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff555e69));
    setColour (juce::TextButton::textColourOffId, cText);
}

void TouchMeLook::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    // dezenter Griff, damit die Ecke der Original-Optik nicht stört
    g.setColour (juce::Colour (0xff24d9f2).withAlpha (dragging ? 0.7f : (over ? 0.45f : 0.12f)));
    for (float k = 0.35f; k < 1.0f; k += 0.3f)
        g.drawLine ((float) w * k, (float) h - 2.0f, (float) w - 2.0f, (float) h * k, 1.2f);
}

juce::Font TouchMeLook::getPopupMenuFont()
{
    int size = 0;
    static juce::Typeface::Ptr tf = [&]
    {
        auto* d = TouchMeAssets::getNamedResource ("ShareTechMonoRegular_ttf", size);
        return d != nullptr ? juce::Typeface::createSystemTypefaceFor (d, (size_t) size) : juce::Typeface::Ptr();
    }();
    if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (20.0f));
    return juce::Font (juce::FontOptions (18.0f));
}

//==============================================================================
TouchMeEditor::TouchMeEditor (TouchMeProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    mono = juce::Typeface::createSystemTypefaceFor (TouchMeAssets::ShareTechMonoRegular_ttf, (size_t) TouchMeAssets::ShareTechMonoRegular_ttfSize);

    using namespace TouchMeAssets;
    imgBg = loadImg (bg_full_png, bg_full_pngSize);
    imgPad = loadImg (xy_pad_clean_png, xy_pad_clean_pngSize);
    imgLineV = loadImg (xy_line_v_png, xy_line_v_pngSize);
    imgLineH = loadImg (xy_line_h_png, xy_line_h_pngSize);
    imgCursor = loadImg (xy_cursor_png, xy_cursor_pngSize);
    imgBtnN = loadImg (btn_normal_png, btn_normal_pngSize);
    imgBtnA = loadImg (btn_active_png, btn_active_pngSize);
    imgPanel = loadImg (fx_panel_fill_png, fx_panel_fill_pngSize);
    imgList = loadImg (preset_list_blank_png, preset_list_blank_pngSize);
    imgThumbScroll = loadImg (scroll_thumb_png, scroll_thumb_pngSize);
    imgCategory = loadImg (category_blank_png, category_blank_pngSize);
    imgPct = loadImg (pct_blank_png, pct_blank_pngSize);
    imgTrackFull = loadImg (track_full_png, track_full_pngSize);
    imgTrackEmpty = loadImg (track_empty_png, track_empty_pngSize);
    imgSliderThumb = loadImg (slider_thumb_png, slider_thumb_pngSize);
    imgTop = loadImg (top_display_blank_png, top_display_blank_pngSize);
    imgXLcd = loadImg (x_lcd_blank_png, x_lcd_blank_pngSize);
    imgYLcd = loadImg (y_lcd_blank_png, y_lcd_blank_pngSize);
    imgWave = loadImg (wave_lcd_blank_png, wave_lcd_blank_pngSize);

    // Statische Ebene einmal in Design-Größe zusammensetzen
    staticLayer = juce::Image (juce::Image::RGB, designW, designH, true);
    {
        juce::Graphics g (staticLayer);
        paintStatic (g);
    }

    loadUserPresets();
    rebuildList();

    setResizable (true, true);
    setResizeLimits (768, 512, 1536, 1024);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (1152, 768);
    startTimerHz (30);
}

TouchMeEditor::~TouchMeEditor() { setLookAndFeel (nullptr); }

juce::Font TouchMeEditor::font (float em, float kernPx, float hScale) const
{
    const float h = em * 1.127f; // Share Tech Mono: Ascent + Descent = 1.127 em
    juce::Font f = mono != nullptr ? juce::Font (juce::FontOptions (mono).withHeight (h))
                                   : juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
    if (std::abs (kernPx) > 1.0e-4f) f.setExtraKerningFactor (kernPx / h);
    if (std::abs (hScale - 1.0f) > 1.0e-4f) f.setHorizontalScale (hScale);
    return f;
}

void TouchMeEditor::resized() { layerDirty = true; }

void TouchMeEditor::setParam (const char* id, float plain)
{
    auto* p = proc.apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

//==============================================================================
// Zeichnen
void TouchMeEditor::paintStatic (juce::Graphics& g)
{
    g.drawImageAt (imgBg, 0, 0);
    g.drawImageAt (imgPanel, 30, 248);
    g.drawImageAt (imgPad, (int) rPad.getX(), (int) rPad.getY());
    g.drawImageAt (imgTop, 1031, 43);
    g.drawImageAt (imgCategory, 1028, 236);
    g.drawImageAt (imgList, 1027, 299);
    g.drawImageAt (imgXLcd, 339, 748);
    g.drawImageAt (imgYLcd, 659, 748);
    g.drawImageAt (imgWave, 1028, 738);
    g.drawImageAt (imgPct, 860, 906);
    g.drawImageAt (juce::ImageCache::getFromMemory (TouchMeAssets::drywet_clean_png, TouchMeAssets::drywet_clean_pngSize), 470, 898);
}

void TouchMeEditor::rebuildLayer()
{
    cachedLayer = juce::Image (juce::Image::RGB, std::max (1, getWidth()), std::max (1, getHeight()), false);
    juce::Graphics g (cachedLayer);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.addTransform (juce::AffineTransform::scale (scale()));
    g.drawImageAt (staticLayer, 0, 0);
    paintSemiStatic (g);
    layerDirty = false;
}

void TouchMeEditor::paint (juce::Graphics& g)
{
    if (layerDirty || cachedLayer.getWidth() != getWidth() || cachedLayer.getHeight() != getHeight())
        rebuildLayer();
    g.drawImageAt (cachedLayer, 0, 0);
    g.addTransform (juce::AffineTransform::scale (scale()));
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    paintDynamic (g);
}

void TouchMeEditor::paintSemiStatic (juce::Graphics& g)
{
    const int prog = proc.activeProgram.load();
    const auto& pr = presets()[(size_t) prog];
    const int fx = pr.cat;

    // ----- FX-Tasten (12 statt 6, gleiche Optik, flacher) -----
    for (int i = 0; i < C_NUM; ++i)
    {
        const auto r = fxRect (i);
        const bool active = i == fx;
        g.drawImage (active ? imgBtnA : imgBtnN, r, juce::RectanglePlacement::stretchToFit);
        if (! active && ((hover == Hit::fx && hoverIndex == i) || (pressed == Hit::fx && pressedIndex == i)))
        {
            g.setColour (juce::Colour (0xff24d9f2).withAlpha (pressed == Hit::fx ? 0.55f : 0.30f));
            g.drawRoundedRectangle (r.reduced (4.0f, 3.5f), 5.0f, 1.4f);
        }
        const auto f = font (19.5f, 0.6f, 1.05f);
        const float base = r.getY() + 23.3f;
        const auto col = active ? cActive : cText;
        glowText (g, juce::String (i + 1).paddedLeft ('0', 2), f, col, 64.0f, base, active ? 0.8f : 0.0f, 0.9f);
        glowText (g, catName (i), f, col, 127.0f, base, active ? 0.8f : 0.0f, 0.9f);
    }

    // ----- HOLD-Lampe -----
    if (param ("hold") > 0.5f)
    {
        const auto led = rHoldLed;
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (juce::Colour (0xff24d9f2).withAlpha (0.10f * (float) (4 - k)));
            g.fillRoundedRectangle (led.expanded ((float) k * 3.0f), 4.0f);
        }
        juce::ColourGradient cg (cCyanHi, led.getCentreX(), led.getY(), juce::Colour (0xff17b8d6), led.getCentreX(), led.getBottom(), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (led, 2.0f);
    }
    if (hover == Hit::hold || pressed == Hit::hold)
    {
        g.setColour (juce::Colour (0xff24d9f2).withAlpha (pressed == Hit::hold ? 0.5f : 0.25f));
        g.drawRoundedRectangle (rHold.reduced (3.0f), 7.0f, 1.4f);
    }

    // ----- Prev / Next / Save: dezentes Cyan beim Überfahren/Drücken -----
    auto btnGlow = [&] (Hit h, juce::Rectangle<float> r)
    {
        if (hover == h || pressed == h)
        {
            if (pressed == h) { g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (r.reduced (4.0f), 6.0f); }
            g.setColour (juce::Colour (0xff24d9f2).withAlpha (pressed == h ? 0.55f : 0.28f));
            g.drawRoundedRectangle (r.reduced (4.0f), 6.0f, 1.4f);
        }
    };
    btnGlow (Hit::prev, rPrev); btnGlow (Hit::next, rNext); btnGlow (Hit::save, rSave);

    // ----- Preset-Anzeige oben -----
    {
        const int urow = selectedUser;
        juce::String txt = urow >= 0 ? "U" + juce::String (urow + 1).paddedLeft ('0', 2) + "  " + userPresets[(size_t) urow].name.toUpperCase()
                                     : juce::String (prog + 1).paddedLeft ('0', 3) + "  " + juce::String (pr.name).toUpperCase();
        float em = 27.5f;
        auto f = font (em, 0.0f, 1.16f);
        const float maxW = 1428.0f - 1120.0f;
        while (textWidth (f, txt) > maxW && em > 17.0f) { em -= 0.5f; f = font (em, 0.0f, 1.16f); }
        glowText (g, txt, f, cCyan, 1120.0f, 94.5f, 1.0f, 1.1f);
        for (auto [h, r] : { std::pair<Hit, juce::Rectangle<float>> { Hit::topPrev, rTopL }, { Hit::topNext, rTopR } })
            if (hover == h || pressed == h)
            {
                g.setColour (juce::Colour (0xff24d9f2).withAlpha (pressed == h ? 0.30f : 0.14f));
                g.fillRoundedRectangle (r.reduced (6.0f, 8.0f), 4.0f);
            }
    }

    // ----- Kategorie -----
    {
        const juce::String c = catFilter < 0 ? "ALL CATEGORIES" : (catFilter == C_NUM ? "USER PRESETS" : juce::String (catName (catFilter)));
        glowText (g, c, font (22.9f, 0.0f, 1.17f), cText, 1061.0f, 270.5f, 0.0f, 0.5f);
        if (hover == Hit::category)
        {
            g.setColour (juce::Colour (0xff24d9f2).withAlpha (0.25f));
            g.drawRoundedRectangle (rCat.reduced (6.0f, 6.0f), 4.0f, 1.2f);
        }
    }

    // ----- Preset-Liste -----
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (rRows.toNearestInt().withHeight (juce::roundToInt (rowPitch * visibleRows) + 2));
        const int sel = selectedRow();
        g.setColour (cSep);
        for (int j = 1; j < visibleRows; ++j)
            g.fillRect (rRows.getX() + 1.0f, rRows.getY() + rowPitch * (float) j - 0.5f, rRows.getWidth() - 2.0f, 1.0f);
        const auto f = font (25.4f);
        for (int r = 0; r < visibleRows; ++r)
        {
            const int idx = scrollRow + r;
            if (idx >= (int) list.size()) break;
            const float top = rRows.getY() + rowPitch * (float) r;
            const bool isSel = idx == sel;
            if (isSel)
            {
                juce::ColourGradient hg (juce::Colour (0xff1fbad1), 0, top, juce::Colour (0xff25c9de), 0, top + 31.0f, false);
                g.setGradientFill (hg);
                g.fillRect (juce::Rectangle<float> (rRows.getX(), top, rRows.getWidth(), 31.5f));
            }
            else if (hover == Hit::list && hoverIndex == r)
            {
                g.setColour (juce::Colour (0xff24d9f2).withAlpha (0.10f));
                g.fillRect (juce::Rectangle<float> (rRows.getX(), top, rRows.getWidth(), 31.5f));
            }
            const auto col = isSel ? cHlText : cText;
            const float base = top + 24.1f;
            glowText (g, entryNumber (list[(size_t) idx]), f, col, 1056.0f, base, 0.0f, 0.6f);
            glowText (g, entryName (list[(size_t) idx]), f, col, 1124.3f, base, 0.0f, 0.6f);
        }
    }
    // Scrollbalken
    {
        const int maxScroll = std::max (0, (int) list.size() - visibleRows);
        const float y0 = 299.0f + 56.0f, y1 = 299.0f + 232.0f - (float) imgThumbScroll.getHeight();
        const float ty = maxScroll > 0 ? juce::jmap ((float) scrollRow, 0.0f, (float) maxScroll, y0, y1) : y0;
        g.drawImageAt (imgThumbScroll, 1027 + 436, juce::roundToInt (ty));
    }

    // ----- X/Y-Beschriftung -----
    glowText (g, axisLabel (pr, true), font (24.3f, 0.0f, 1.04f), cLabel, 368.0f, 788.5f, 0.0f, 0.8f);
    glowText (g, axisLabel (pr, false), font (24.3f, 0.0f, 1.04f), cLabel, 685.0f, 788.5f, 0.0f, 0.8f);
}

void TouchMeEditor::paintDynamic (juce::Graphics& g)
{
    const int prog = proc.activeProgram.load();
    const auto& pr = presets()[(size_t) prog];
    const float x = param ("x"), y = param ("y");
    const bool on = param ("touch") > 0.5f || param ("hold") > 0.5f;

    // ----- XY-Pad: Fadenkreuz + Cursor -----
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (rPad.toNearestInt());
        const float px = rPad.getX() + x * rPad.getWidth();
        const float py = rPad.getY() + (1.0f - y) * rPad.getHeight();
        const float lw = (float) imgLineV.getWidth(), lh = (float) imgLineH.getHeight();
        g.setOpacity (1.0f);
        g.drawImage (imgLineV, juce::Rectangle<float> (px - lw * 0.5f, rPad.getY(), lw, rPad.getHeight()), juce::RectanglePlacement::stretchToFit);
        g.drawImage (imgLineH, juce::Rectangle<float> (rPad.getX(), py - lh * 0.5f, rPad.getWidth(), lh), juce::RectanglePlacement::stretchToFit);
        // weiches Leuchten um den Punkt wie im Original
        juce::ColourGradient halo (juce::Colour (0xff24d9f2).withAlpha (on ? 0.32f : 0.18f), px, py,
                                   juce::Colour (0xff24d9f2).withAlpha (0.0f), px + 44.0f, py, true);
        g.setGradientFill (halo);
        g.fillEllipse (px - 44.0f, py - 44.0f, 88.0f, 88.0f);
        const float cw = (float) imgCursor.getWidth(), ch = (float) imgCursor.getHeight();
        g.drawImage (imgCursor, juce::Rectangle<float> (px - cw * 0.5f, py - ch * 0.5f, cw, ch), juce::RectanglePlacement::stretchToFit);
        if (on)
        {
            const float env = proc.envMeter.load();
            g.setColour (cCyanHi.withAlpha (0.25f + 0.35f * env));
            g.drawEllipse (px - 30.0f, py - 30.0f, 60.0f, 60.0f, 1.5f);
        }
    }

    // ----- X/Y-Werte -----
    auto value = [&] (const juce::String& s, float lcdX)
    {
        float em = 40.0f;
        auto f = font (em, 0.0f, 1.15f);
        const float maxW = 309.0f - 72.0f - 14.0f;
        while (textWidth (f, s) > maxW && em > 22.0f) { em -= 1.0f; f = font (em, 0.0f, 1.15f); }
        glowText (g, s, f, cCyanHi, lcdX + 72.0f, 839.5f, 1.0f, 1.4f);
    };
    value (axisValue (pr, true, x), 339.0f);
    value (axisValue (pr, false, y), 659.0f);

    // ----- Dry/Wet -----
    {
        const float mix = param ("dryWet");
        const float c = thumbC0 + mix * thumbSpan;
        g.drawImageAt (imgTrackEmpty, (int) rTrack.getX(), (int) rTrack.getY());
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (juce::Rectangle<int> ((int) rTrack.getX(), (int) rTrack.getY(), juce::roundToInt (c - rTrack.getX()), (int) rTrack.getHeight()));
            g.drawImageAt (imgTrackFull, (int) rTrack.getX(), (int) rTrack.getY());
        }
        g.drawImage (imgSliderThumb, juce::Rectangle<float> (c - 25.0f, 907.0f, 50.0f, 57.0f), juce::RectanglePlacement::stretchToFit);
        const juce::String s = juce::String (juce::roundToInt (mix * 100.0f)) + " %";
        const auto f = font (28.6f);
        glowText (g, s, f, cCyan, 911.0f - textWidth (f, s) * 0.5f + 3.0f, 940.5f, 0.8f, 1.0f);
    }

    drawWave (g);
}

void TouchMeEditor::drawWave (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (rWave.toNearestInt());
    const auto& w = proc.wave;
    const int wp = w.writePos.load (std::memory_order_acquire);
    const int N = 458;
    const float x0 = 1040.0f, cy = 837.0f, amp = 74.0f;
    juce::Path path;
    for (int k = 0; k < N; ++k)
    {
        const int i = (wp - N + k + WaveMonitor::size * 4) % WaveMonitor::size;
        const float hi = juce::jlimit (-1.0f, 1.0f, w.mx[i] * 1.6f), lo = juce::jlimit (-1.0f, 1.0f, w.mn[i] * 1.6f);
        const float xx = x0 + (float) k;
        if (k == 0) path.startNewSubPath (xx, cy - hi * amp);
        else path.lineTo (xx, cy - hi * amp);
        path.lineTo (xx + 0.5f, cy - lo * amp);
    }
    g.setColour (cWave.withAlpha (0.10f));
    g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cWave.withAlpha (0.28f));
    g.strokePath (path, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cWave);
    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
// Zustand beobachten
void TouchMeEditor::timerCallback()
{
    const int prog = proc.activeProgram.load();
    const int fx = presets()[(size_t) prog].cat;
    const float mix = param ("dryWet");
    const bool hold = param ("hold") > 0.5f;
    if (selectedUser >= 0 && (selectedUser >= (int) userPresets.size() || userPresets[(size_t) selectedUser].program != prog))
        selectedUser = -1;
    if (prog != shownProgram || fx != shownFx || hold != shownHold || selectedUser != shownUser)
    {
        if (prog != shownProgram) ensureVisible();
        shownProgram = prog; shownFx = fx; shownHold = hold; shownUser = selectedUser;
        layerDirty = true;
    }
    shownMix = mix;
    repaint();
}

//==============================================================================
// Presets und Liste
void TouchMeEditor::rebuildList()
{
    list.clear();
    if (catFilter != C_NUM)
        for (int i = 0; i < numPresets(); ++i)
            if (catFilter < 0 || presets()[(size_t) i].cat == catFilter) list.push_back ({ false, i });
    if (catFilter < 0 || catFilter == C_NUM)
        for (int i = 0; i < (int) userPresets.size(); ++i) list.push_back ({ true, i });
    scrollRow = juce::jlimit (0, std::max (0, (int) list.size() - visibleRows), scrollRow);
    layerDirty = true;
}

int TouchMeEditor::selectedRow() const
{
    const int prog = proc.activeProgram.load();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& e = list[(size_t) i];
        if (selectedUser >= 0 ? (e.user && e.index == selectedUser) : (! e.user && e.index == prog)) return i;
    }
    return -1;
}

void TouchMeEditor::ensureVisible()
{
    const int r = selectedRow();
    if (r < 0) return;
    if (r < scrollRow) scrollRow = r;
    if (r >= scrollRow + visibleRows) scrollRow = r - visibleRows + 1;
    layerDirty = true;
}

juce::String TouchMeEditor::entryNumber (const Entry& e) const
{
    return e.user ? "U" + juce::String (e.index + 1).paddedLeft ('0', 2) : juce::String (e.index + 1).paddedLeft ('0', 3);
}

juce::String TouchMeEditor::entryName (const Entry& e) const
{
    return e.user ? userPresets[(size_t) e.index].name : juce::String (presets()[(size_t) e.index].name);
}

void TouchMeEditor::selectEntry (const Entry& e)
{
    if (e.user)
    {
        const auto& u = userPresets[(size_t) e.index];
        proc.selectProgram (u.program, false);
        for (auto [id, v] : { std::pair<const char*, float> { "x", u.x }, { "y", u.y }, { "dryWet", u.mix }, { "hold", u.hold ? 1.0f : 0.0f } })
        {
            auto* p = proc.apvts.getParameter (id);
            p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture();
        }
        selectedUser = e.index;
    }
    else
    {
        selectedUser = -1;
        proc.selectProgram (e.index, true);
    }
    proc.activeProgram.store (e.user ? userPresets[(size_t) e.index].program : e.index); // Anzeige sofort umschalten
    ensureVisible();
    layerDirty = true;
}

void TouchMeEditor::stepPreset (int dir)
{
    if (list.empty()) return;
    int r = selectedRow();
    r = r < 0 ? 0 : (r + dir + (int) list.size()) % (int) list.size();
    selectEntry (list[(size_t) r]);
}

void TouchMeEditor::showCategoryMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "ALL CATEGORIES", true, catFilter < 0);
    m.addSeparator();
    for (int c = 0; c < C_NUM; ++c) m.addItem (c + 2, juce::String (c + 1).paddedLeft ('0', 2) + "  " + catName (c), true, catFilter == c);
    m.addSeparator();
    m.addItem (C_NUM + 2, "USER PRESETS", true, catFilter == C_NUM);
    const auto area = localAreaToGlobal ((rCat * scale()).toNearestInt());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area).withMinimumWidth (area.getWidth()),
                     [safe = juce::Component::SafePointer<TouchMeEditor> (this)] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         safe->catFilter = r == 1 ? -1 : r - 2;
                         safe->scrollRow = 0;
                         safe->rebuildList();
                         safe->ensureVisible();
                     });
}

juce::File TouchMeEditor::userPresetFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("BrainBugz").getChildFile ("TouchMe").getChildFile ("UserPresets.json");
}

void TouchMeEditor::loadUserPresets()
{
    userPresets.clear();
    const auto json = juce::JSON::parse (userPresetFile());
    if (auto* arr = json.getArray())
        for (auto& v : *arr)
        {
            UserPreset u;
            u.name = v.getProperty ("name", "User").toString();
            u.program = juce::jlimit (0, numPresets() - 1, (int) v.getProperty ("preset", 0));
            u.x = (float) (double) v.getProperty ("x", 0.5);
            u.y = (float) (double) v.getProperty ("y", 0.5);
            u.mix = (float) (double) v.getProperty ("mix", 1.0);
            u.hold = (bool) v.getProperty ("hold", false);
            userPresets.push_back (u);
        }
}

void TouchMeEditor::storeUserPresets()
{
    juce::Array<juce::var> arr;
    for (auto& u : userPresets)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", u.name);
        o->setProperty ("preset", u.program);
        o->setProperty ("fx", juce::String (catName (presets()[(size_t) u.program].cat)));
        o->setProperty ("x", u.x); o->setProperty ("y", u.y);
        o->setProperty ("mix", u.mix); o->setProperty ("hold", u.hold);
        arr.add (juce::var (o));
    }
    auto f = userPresetFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (juce::var (arr)));
}

void TouchMeEditor::savePreset()
{
    const int prog = proc.activeProgram.load();
    auto* w = new juce::AlertWindow ("SAVE PRESET", "Name fuer das neue User-Preset:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&look);
    w->addTextEditor ("name", juce::String (presets()[(size_t) prog].name) + " 2");
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<TouchMeEditor> (this), w, prog] (int r)
        {
            if (safe == nullptr || r != 1) return;
            auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty()) name = "User";
            UserPreset u { name.substring (0, 24), prog, safe->param ("x"), safe->param ("y"), safe->param ("dryWet"), safe->param ("hold") > 0.5f };
            safe->userPresets.push_back (u);
            safe->storeUserPresets();
            safe->selectedUser = (int) safe->userPresets.size() - 1;
            safe->rebuildList();
            safe->ensureVisible();
        }), true);
}

//==============================================================================
// Maus
TouchMeEditor::Hit TouchMeEditor::hitTest (juce::Point<float> d, int& index) const
{
    index = -1;
    if (rPad.contains (d)) return Hit::pad;
    for (int i = 0; i < C_NUM; ++i) if (fxRect (i).contains (d)) { index = i; return Hit::fx; }
    if (rHold.contains (d)) return Hit::hold;
    if (rPrev.contains (d)) return Hit::prev;
    if (rNext.contains (d)) return Hit::next;
    if (rSave.contains (d)) return Hit::save;
    if (rTopL.contains (d)) return Hit::topPrev;
    if (rTopR.contains (d)) return Hit::topNext;
    if (rCat.contains (d)) return Hit::category;
    if (rRows.contains (d)) { index = (int) ((d.y - rRows.getY()) / rowPitch); return Hit::list; }
    if (rScrUp.contains (d)) return Hit::scrollUp;
    if (rScrDn.contains (d)) return Hit::scrollDown;
    if (rScrTrk.contains (d)) return Hit::scrollTrack;
    if (rSlider.contains (d)) return Hit::slider;
    return Hit::none;
}

void TouchMeEditor::setPadFromMouse (juce::Point<float> d)
{
    setParam ("x", clamp01 ((d.x - rPad.getX()) / rPad.getWidth()));
    setParam ("y", clamp01 (1.0f - (d.y - rPad.getY()) / rPad.getHeight()));
}

void TouchMeEditor::setMixFromMouse (juce::Point<float> d)
{
    setParam ("dryWet", clamp01 ((d.x - thumbC0) / thumbSpan));
}

void TouchMeEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);
    int idx = -1;
    const auto h = hitTest (d, idx);
    pressed = h; pressedIndex = idx;
    switch (h)
    {
        case Hit::pad:
            padDown = true;
            for (auto* id : { "x", "y", "touch" }) proc.apvts.getParameter (id)->beginChangeGesture();
            setPadFromMouse (d);
            setParam ("touch", 1.0f);
            break;
        case Hit::fx:
        {
            const int prog = proc.activeProgram.load();
            int target = firstOfCategory (idx);
            if (presets()[(size_t) prog].cat == idx) // gleiche Taste nochmal: nächstes Preset der Kategorie
                for (int i = 1; i <= numPresets(); ++i)
                {
                    const int k = (prog + i) % numPresets();
                    if (presets()[(size_t) k].cat == idx) { target = k; break; }
                }
            catFilter = idx; rebuildList();
            selectEntry ({ false, target });
            break;
        }
        case Hit::hold:
        {
            auto* p = proc.apvts.getParameter ("hold");
            p->beginChangeGesture(); p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); p->endChangeGesture();
            break;
        }
        case Hit::prev: case Hit::topPrev: stepPreset (-1); break;
        case Hit::next: case Hit::topNext: stepPreset (1); break;
        case Hit::save: savePreset(); break;
        case Hit::category: showCategoryMenu(); break;
        case Hit::list:
            if (scrollRow + idx < (int) list.size()) selectEntry (list[(size_t) (scrollRow + idx)]);
            break;
        case Hit::scrollUp: scrollRow = std::max (0, scrollRow - 1); break;
        case Hit::scrollDown: scrollRow = std::min (std::max (0, (int) list.size() - visibleRows), scrollRow + 1); break;
        case Hit::scrollTrack:
        {
            const int maxScroll = std::max (0, (int) list.size() - visibleRows);
            scrollRow = juce::roundToInt (juce::jmap (d.y, rScrTrk.getY(), rScrTrk.getBottom(), 0.0f, (float) maxScroll));
            scrollRow = juce::jlimit (0, maxScroll, scrollRow);
            break;
        }
        case Hit::slider:
            proc.apvts.getParameter ("dryWet")->beginChangeGesture();
            setMixFromMouse (d);
            break;
        case Hit::none: break;
    }
    layerDirty = true;
}

void TouchMeEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);
    if (padDown) setPadFromMouse (d);
    else if (pressed == Hit::slider) setMixFromMouse (d);
    else if (pressed == Hit::scrollTrack)
    {
        const int maxScroll = std::max (0, (int) list.size() - visibleRows);
        scrollRow = juce::jlimit (0, maxScroll, juce::roundToInt (juce::jmap (d.y, rScrTrk.getY(), rScrTrk.getBottom(), 0.0f, (float) maxScroll)));
        layerDirty = true;
    }
}

void TouchMeEditor::mouseUp (const juce::MouseEvent&)
{
    if (padDown)
    {
        padDown = false;
        setParam ("touch", 0.0f);
        for (auto* id : { "x", "y", "touch" }) proc.apvts.getParameter (id)->endChangeGesture();
    }
    if (pressed == Hit::slider) proc.apvts.getParameter ("dryWet")->endChangeGesture();
    pressed = Hit::none; pressedIndex = -1;
    layerDirty = true;
}

void TouchMeEditor::mouseMove (const juce::MouseEvent& e)
{
    int idx = -1;
    const auto h = hitTest (toDesign (e.position), idx);
    if (h != hover || idx != hoverIndex) { hover = h; hoverIndex = idx; layerDirty = true; }
    setMouseCursor (h == Hit::pad ? juce::MouseCursor::CrosshairCursor
                                  : (h == Hit::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor));
}

void TouchMeEditor::mouseExit (const juce::MouseEvent&)
{
    if (hover != Hit::none) { hover = Hit::none; hoverIndex = -1; layerDirty = true; }
}

void TouchMeEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto d = toDesign (e.position);
    if (d.x > 1020.0f && d.y > 295.0f && d.y < 580.0f)
    {
        const int maxScroll = std::max (0, (int) list.size() - visibleRows);
        scrollRow = juce::jlimit (0, maxScroll, scrollRow + (w.deltaY > 0 ? -1 : 1));
        layerDirty = true;
    }
}

juce::AudioProcessorEditor* TouchMeProcessor::createEditor() { return new TouchMeEditor (*this); }
