#include "Analyser.h"

namespace bfz::ui
{

Analyser::Analyser (BrainfuzzProcessor& p) : proc (p)
{
    fftBuf.assign ((size_t) kSize * 2, 0.0f);
    setRepaintsOnMouseActivity (false);
    setWantsKeyboardFocus (false);
}

// ----------------------------------------------------------------- Geometrie

juce::Rectangle<float> Analyser::plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedTop (22.0f).withTrimmedBottom (14.0f); }

float Analyser::maxF() const { return (float) std::min (20000.0, proc.sampleRateViz.load() * 0.5); }

float Analyser::freqToX (float f) const
{
    const auto r = plotArea();
    const float t = std::log (std::max (f, kMinF) / kMinF) / std::log (maxF() / kMinF);
    return r.getX() + t * r.getWidth();
}

float Analyser::xToFreq (float x) const
{
    const auto r = plotArea();
    const float t = (x - r.getX()) / r.getWidth();
    return kMinF * std::pow (maxF() / kMinF, juce::jlimit (0.0f, 1.0f, t));
}

float Analyser::dbToY (float db) const
{
    const auto r = plotArea();
    return juce::jmap (db, kMinDb, kMaxDb, r.getBottom(), r.getY());
}

float Analyser::driveToY (float d) const
{
    const auto r = plotArea();
    return juce::jmap (d, 0.0f, kMaxDrive, r.getBottom() - 34.0f, r.getY() + 26.0f);
}

float Analyser::yToDrive (float y) const
{
    const auto r = plotArea();
    return juce::jlimit (0.0f, kMaxDrive, juce::jmap (y, r.getBottom() - 34.0f, r.getY() + 26.0f, 0.0f, kMaxDrive));
}

float Analyser::bandLeftX (int b) const
{
    return b == 0 ? plotArea().getX() : freqToX (proc.value (pX1 + b - 1));
}

float Analyser::bandRightX (int b) const
{
    return b == 3 ? plotArea().getRight() : freqToX (proc.value (pX1 + b));
}

juce::Rectangle<float> Analyser::xLabelRect (int k) const
{
    const float x = freqToX (proc.value (pX1 + k));
    return { x - 33.0f, 4.0f, 66.0f, 18.0f };
}

juce::Point<float> Analyser::handlePos (int b) const
{
    const float x = 0.5f * (bandLeftX (b) + bandRightX (b));
    return { x, driveToY (proc.value (bandIndex (b, bDrive))) };
}

juce::Rectangle<float> Analyser::driveLabelRect (int b) const
{
    const auto h = handlePos (b);
    return { h.x - 54.0f, h.y + 14.0f, 108.0f, 16.0f };
}

Analyser::Hit Analyser::hitTest (juce::Point<float> p) const
{
    for (int k = 0; k < 3; ++k) if (xLabelRect (k).contains (p)) return { HitKind::xLabel, k };
    for (int b = 0; b < 4; ++b)
    {
        if (handlePos (b).getDistanceFrom (p) < 13.0f) return { HitKind::drive, b };
        if (driveLabelRect (b).contains (p)) return { HitKind::dLabel, b };
    }
    const auto r = plotArea();
    if (p.y >= r.getY() - 4.0f)
        for (int k = 0; k < 3; ++k)
            if (std::abs (p.x - freqToX (proc.value (pX1 + k))) < 6.0f) return { HitKind::xover, k };
    for (int b = 0; b < 4; ++b)
        if (p.x > bandLeftX (b) + 6.0f && p.x < bandRightX (b) - 6.0f && std::abs (p.y - handlePos (b).y) < 6.0f)
            return { HitKind::drive, b };
    return {};
}

// ----------------------------------------------------------------- Spektrum

void Analyser::computeSpectrum (const VizRing<BrainfuzzProcessor::kSpecRing>& ring, std::vector<float>& smooth)
{
    const auto r = plotArea();
    const int w = std::max (2, (int) r.getWidth());
    if ((int) smooth.size() != w) smooth.assign ((size_t) w, kMinDb);

    ring.read (fftBuf.data(), kSize);
    std::fill (fftBuf.begin() + kSize, fftBuf.end(), 0.0f);
    window.multiplyWithWindowingTable (fftBuf.data(), (size_t) kSize);
    fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);

    const float sr = (float) proc.sampleRateViz.load();
    const float binHz = sr / (float) kSize;
    const float norm = 4.0f / (float) kSize;
    for (int x = 0; x < w; ++x)
    {
        const float f0 = xToFreq (r.getX() + x - 0.5f), f1 = xToFreq (r.getX() + x + 0.5f);
        const float b0 = f0 / binHz, b1 = f1 / binHz;
        float mag = 0.0f;
        if (b1 - b0 < 1.0f)
        {
            // Interpolieren (tiefe Frequenzen, wenige Bins pro Pixel)
            const float bc = 0.5f * (b0 + b1);
            const int i = juce::jlimit (0, kBins - 2, (int) bc);
            const float fr = bc - (float) i;
            mag = fftBuf[(size_t) i] + (fftBuf[(size_t) i + 1] - fftBuf[(size_t) i]) * fr;
        }
        else
            for (int i = juce::jlimit (0, kBins - 1, (int) b0); i <= juce::jlimit (0, kBins - 1, (int) b1); ++i)
                mag = std::max (mag, fftBuf[(size_t) i]);
        const float fc = 0.5f * (f0 + f1);
        // +4.5 dB/Okt. Neigung um 1 kHz: Musik wirkt dann ungefaehr flach
        float db = juce::Decibels::gainToDecibels (mag * norm, -140.0f) + 4.5f * std::log2 (fc / 1000.0f);
        db = juce::jlimit (kMinDb - 10.0f, kMaxDb + 6.0f, db);
        auto& s = smooth[(size_t) x];
        s = db > s ? s + (db - s) * 0.6f : s + (db - s) * 0.12f;
    }
}

juce::Path Analyser::spectrumPath (const std::vector<float>& db, bool closed) const
{
    const auto r = plotArea();
    juce::Path p;
    if (db.empty()) return p;
    if (closed) p.startNewSubPath (r.getX(), r.getBottom());
    for (size_t x = 0; x < db.size(); ++x)
    {
        const float y = juce::jlimit (r.getY(), r.getBottom(), dbToY (db[x]));
        if (! closed && x == 0) p.startNewSubPath (r.getX(), y);
        else p.lineTo (r.getX() + (float) x, y);
    }
    if (closed) { p.lineTo (r.getRight(), r.getBottom()); p.closeSubPath(); }
    return p;
}

void Analyser::update()
{
    for (int b = 0; b < 4; ++b) computeSpectrum (proc.bandSpec[(size_t) b], bandDb[(size_t) b]);
    computeSpectrum (proc.outSpec, outDb);
    repaint();
}

// ----------------------------------------------------------------- Zeichnen

void Analyser::resized()
{
    background = juce::Image (juce::Image::ARGB, std::max (1, getWidth() * 2), std::max (1, getHeight() * 2), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (2.0f));
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bg (col::bg1, 0, 0, col::bg0, 0, bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto r = plotArea();
    // Retro-Sonne hinter dem Spektrum
    {
        const float cx = r.getCentreX(), cy = r.getBottom() + r.getHeight() * 0.15f, rad = r.getHeight() * 0.75f;
        juce::ColourGradient sun (col::pink.withAlpha (0.10f), cx, cy - rad, col::violet.withAlpha (0.0f), cx, cy + rad * 0.2f, false);
        sun.addColour (0.4, col::orange.withAlpha (0.06f));
        g.setGradientFill (sun);
        g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        // Streifen ausschneiden (Synthwave-Sonne)
        g.setColour (col::bg0.withAlpha (0.75f));
        for (int i = 0; i < 6; ++i)
        {
            const float y = cy - rad * 0.12f - i * rad * 0.11f;
            g.fillRect (cx - rad, y, rad * 2.0f, 1.5f + i * 0.5f);
        }
    }
    // Perspektiv-Boden
    {
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        const float horizon = r.getBottom() - r.getHeight() * 0.22f;
        for (int i = 0; i < 7; ++i)
        {
            const float t = (float) i / 6.0f;
            const float y = horizon + (r.getBottom() - horizon) * t * t;
            g.setColour (col::pink.withAlpha (0.05f + 0.07f * t));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
        for (int i = -12; i <= 12; ++i)
        {
            const float x0 = r.getCentreX() + i * r.getWidth() * 0.02f;
            const float x1 = r.getCentreX() + i * r.getWidth() * 0.12f;
            g.setColour (col::pink.withAlpha (0.06f));
            g.drawLine (x0, horizon, x1, r.getBottom(), 1.0f);
        }
        g.restoreState();
    }
    // Raster
    g.setFont (fonts::mono (10.0f));
    for (float db = 0.0f; db > kMinDb; db -= 12.0f)
    {
        const float y = dbToY (db);
        g.setColour (col::grid.withAlpha (0.8f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }
    const float freqs[] { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* names[] { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i)
    {
        if (freqs[i] > maxF()) continue;
        const float x = freqToX (freqs[i]);
        g.setColour (col::grid);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (col::inkMute);
        g.drawText (names[i], juce::Rectangle<float> (x - 20.0f, r.getBottom() + 1.0f, 40.0f, 12.0f), juce::Justification::centred);
    }
    g.setColour (col::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
}

void Analyser::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());
    const auto r = plotArea();

    bool anySolo = false;
    for (int b = 0; b < 4; ++b) anySolo = anySolo || proc.value (bandIndex (b, bSolo)) > 0.5f;

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());

    // Band-Kaesten (Hoehe = Drive)
    for (int b = 0; b < 4; ++b)
    {
        const auto c = col::band (b);
        const bool muted = proc.value (bandIndex (b, bMute)) > 0.5f || (anySolo && proc.value (bandIndex (b, bSolo)) < 0.5f);
        const bool byp = proc.value (bandIndex (b, bBypass)) > 0.5f;
        const float a = muted ? 0.3f : 1.0f;
        const float x0 = bandLeftX (b), x1 = bandRightX (b);
        const float top = handlePos (b).y;
        juce::Rectangle<float> box (x0 + 1.0f, top, x1 - x0 - 2.0f, r.getBottom() - top);
        juce::ColourGradient fill (c.withAlpha (0.20f * a), 0, top, c.withAlpha (0.02f * a), 0, r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRect (box);
        if (byp)
        {
            g.setColour (c.withAlpha (0.12f));
            for (float x = x0 - r.getHeight(); x < x1; x += 10.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
        }
    }

    // Spektren pro Band
    for (int b = 0; b < 4; ++b)
    {
        const auto c = col::band (b);
        const bool muted = proc.value (bandIndex (b, bMute)) > 0.5f || (anySolo && proc.value (bandIndex (b, bSolo)) < 0.5f);
        const float a = muted ? 0.25f : 1.0f;
        const auto& db = bandDb[(size_t) b];
        if (db.empty()) continue;
        auto filled = spectrumPath (db, true);
        juce::ColourGradient grad (c.withAlpha (0.38f * a), 0, r.getY(), c.withAlpha (0.04f * a), 0, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (filled);
        auto line = spectrumPath (db, false);
        g.setColour (c.withAlpha (0.28f * a));
        g.strokePath (line, juce::PathStrokeType (3.5f));
        g.setColour (c.withAlpha (0.95f * a));
        g.strokePath (line, juce::PathStrokeType (1.3f));
    }
    // Gesamt-Ausgang
    if (! outDb.empty())
    {
        g.setColour (col::ink.withAlpha (0.55f));
        g.strokePath (spectrumPath (outDb, false), juce::PathStrokeType (1.0f));
    }

    // Kanten und Griffe
    for (int b = 0; b < 4; ++b)
    {
        const auto c = col::band (b);
        const float x0 = bandLeftX (b), x1 = bandRightX (b);
        const auto h = handlePos (b);
        const bool hot = (hover.kind == HitKind::drive && hover.index == b) || (active.kind == HitKind::drive && active.index == b);
        g.setColour (c.withAlpha (0.3f));
        g.fillRect (x0 + 1.0f, h.y - 2.0f, x1 - x0 - 2.0f, 4.0f);
        g.setColour (c);
        g.fillRect (x0 + 1.0f, h.y - 1.0f, x1 - x0 - 2.0f, 2.0f);

        const float hr = hot ? 12.0f : 10.5f;
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (h.x - hr - 4.0f, h.y - hr - 4.0f, (hr + 4.0f) * 2.0f, (hr + 4.0f) * 2.0f);
        g.setColour (col::bg0);
        g.fillEllipse (h.x - hr, h.y - hr, hr * 2.0f, hr * 2.0f);
        g.setColour (c);
        g.drawEllipse (h.x - hr, h.y - hr, hr * 2.0f, hr * 2.0f, 2.0f);
        g.setFont (fonts::display (13.0f, true));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (h.x - hr, h.y - hr, hr * 2.0f, hr * 2.0f), juce::Justification::centred);

        // Drive-Wert
        if (x1 - x0 > 100.0f)
        {
            const auto lr = driveLabelRect (b);
            const bool lhot = hover.kind == HitKind::dLabel && hover.index == b;
            g.setColour (col::bg0.withAlpha (0.75f));
            g.fillRoundedRectangle (lr, 4.0f);
            if (lhot) { g.setColour (c.withAlpha (0.6f)); g.drawRoundedRectangle (lr, 4.0f, 1.0f); }
            g.setColour (col::ink);
            g.setFont (fonts::mono (12.0f));
            g.drawText ("Drive " + BrainfuzzProcessor::formatValue (paramSpecs()[(size_t) bandIndex (b, bDrive)],
                                                                   proc.value (bandIndex (b, bDrive))),
                        lr, juce::Justification::centred);
        }
    }
    g.restoreState();

    // Weichen-Linien und Frequenz-Pillen
    for (int k = 0; k < 3; ++k)
    {
        const float x = freqToX (proc.value (pX1 + k));
        const bool hot = (hover.index == k && (hover.kind == HitKind::xover || hover.kind == HitKind::xLabel))
                      || (active.index == k && active.kind == HitKind::xover);
        g.setColour (col::ink.withAlpha (hot ? 0.9f : 0.45f));
        g.drawLine (x, r.getY() - 4.0f, x, r.getBottom(), hot ? 2.0f : 1.2f);
        const auto lr = xLabelRect (k);
        g.setColour (col::bg0);
        g.fillRoundedRectangle (lr, 5.0f);
        g.setGradientFill (juce::ColourGradient (col::band (k), lr.getX(), 0, col::band (k + 1), lr.getRight(), 0, false));
        g.drawRoundedRectangle (lr.reduced (0.5f), 5.0f, hot ? 1.6f : 1.0f);
        g.setColour (col::ink);
        g.setFont (fonts::mono (12.0f));
        g.drawText (BrainfuzzProcessor::formatValue (paramSpecs()[(size_t) (pX1 + k)], proc.value (pX1 + k)), lr, juce::Justification::centred);
    }

    // Eckbeschriftung
    g.setColour (col::inkMute);
    g.setFont (fonts::mono (10.0f));
    g.drawText ("20 Hz", juce::Rectangle<float> (r.getX() + 2.0f, r.getBottom() - 14.0f, 50.0f, 12.0f), juce::Justification::left);
    g.drawText (maxF() >= 19999.0f ? "20 kHz" : juce::String (maxF() / 1000.0f, 1) + " kHz",
                juce::Rectangle<float> (r.getRight() - 52.0f, r.getBottom() - 14.0f, 50.0f, 12.0f), juce::Justification::right);
}

// ----------------------------------------------------------------- Maus

void Analyser::beginEdit (int paramIndex)
{
    editingParam = paramIndex;
    proc.undo.beginNewTransaction();
    proc.param (paramIndex)->beginChangeGesture();
    dragStartValue = proc.value (paramIndex);
}

void Analyser::endEdit()
{
    if (editingParam >= 0) proc.param (editingParam)->endChangeGesture();
    editingParam = -1;
}

void Analyser::editValue (int paramIndex, juce::Rectangle<float> where)
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (fonts::mono (13.0f));
    editor->setJustification (juce::Justification::centred);
    editor->setText (proc.param (paramIndex)->getCurrentValueAsText(), false);
    editor->selectAll();
    editor->setBounds (where.expanded (4.0f, 3.0f).toNearestInt());
    auto done = [this, paramIndex] (bool apply)
    {
        if (editor == nullptr) return;
        if (apply)
        {
            auto* p = proc.param (paramIndex);
            const float v = p->getValueForText (editor->getText());
            proc.undo.beginNewTransaction();
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Analyser> (this)]
        {
            if (safe != nullptr) { safe->editor.reset(); safe->repaint(); }
        });
    };
    editor->onReturnKey = [done] { done (true); };
    editor->onEscapeKey = [done] { done (false); };
    editor->onFocusLost = [done] { done (true); };
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
}

void Analyser::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    if (h.kind != hover.kind || h.index != hover.index) { hover = h; repaint(); }
    switch (h.kind)
    {
        case HitKind::xover:  setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case HitKind::drive:  setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case HitKind::xLabel:
        case HitKind::dLabel: setMouseCursor (juce::MouseCursor::IBeamCursor); break;
        case HitKind::none:   setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void Analyser::mouseExit (const juce::MouseEvent&) { hover = {}; repaint(); }

void Analyser::mouseDown (const juce::MouseEvent& e)
{
    active = hitTest (e.position);
    dragStart = e.position;
    if (active.kind == HitKind::xover) beginEdit (pX1 + active.index);
    else if (active.kind == HitKind::drive) beginEdit (bandIndex (active.index, bDrive));
}

void Analyser::mouseDrag (const juce::MouseEvent& e)
{
    if (editingParam < 0) return;
    const bool fine = e.mods.isShiftDown();
    auto* p = proc.param (editingParam);
    if (active.kind == HitKind::xover)
    {
        const int k = active.index;
        const float startX = freqToX (dragStartValue);
        float x = startX + (e.position.x - dragStart.x) * (fine ? 0.2f : 1.0f);
        float f = xToFreq (x);
        // Reihenfolge der Weichen halten
        if (k > 0) f = std::max (f, proc.value (pX1 + k - 1) * 1.26f);
        if (k < 2) f = std::min (f, proc.value (pX1 + k + 1) / 1.26f);
        p->setValueNotifyingHost (p->convertTo0to1 (f));
    }
    else if (active.kind == HitKind::drive)
    {
        const float startY = driveToY (dragStartValue);
        const float y = startY + (e.position.y - dragStart.y) * (fine ? 0.2f : 1.0f);
        p->setValueNotifyingHost (p->convertTo0to1 (yToDrive (y)));
    }
    repaint();
}

void Analyser::mouseUp (const juce::MouseEvent& e)
{
    const bool clicked = e.mouseWasClicked();
    const auto a = active;
    endEdit();
    active = {};
    if (clicked && a.kind == HitKind::xLabel) editValue (pX1 + a.index, xLabelRect (a.index));
    if (clicked && a.kind == HitKind::dLabel) editValue (bandIndex (a.index, bDrive), driveLabelRect (a.index));
    repaint();
}

void Analyser::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    int idx = -1;
    if (h.kind == HitKind::xover) idx = pX1 + h.index;
    if (h.kind == HitKind::drive) idx = bandIndex (h.index, bDrive);
    if (idx < 0) return;
    endEdit();
    proc.undo.beginNewTransaction();
    proc.setValue (idx, paramSpecs()[(size_t) idx].def);
}

void Analyser::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto r = plotArea();
    if (! r.contains (e.position)) return;
    for (int b = 0; b < 4; ++b)
        if (e.position.x >= bandLeftX (b) && e.position.x < bandRightX (b))
        {
            const int idx = bandIndex (b, bDrive);
            const float step = (e.mods.isShiftDown() ? 0.1f : 1.0f) * (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f);
            proc.setValue (idx, juce::jlimit (0.0f, kMaxDrive, proc.value (idx) + step));
            break;
        }
}

} // namespace bfz::ui
