#include "StepGrid.h"
#include "PluginEditor.h"

int StepGrid::maxSemis() const
{
    const int range = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::arpRange)->load());
    return juce::jmax (12, 12 * range);
}

juce::Rectangle<float> StepGrid::column (int i) const
{
    const float gap = 3.0f;
    const float w = ((float) getWidth() - gap * 15.0f) / 16.0f;
    return { (float) i * (w + gap), 0.0f, w, (float) getHeight() };
}

int StepGrid::stepAt (float x) const
{
    const float gap = 3.0f;
    const float w = ((float) getWidth() - gap * 15.0f) / 16.0f;
    return juce::jlimit (0, 15, (int) (x / (w + gap)));
}

void StepGrid::setPitchFromY (int step, float y)
{
    const float rel = 1.0f - (y - pitchTop()) / (pitchBottom() - pitchTop());
    auto st = proc.arp.getStep (step);
    st.semis = juce::jlimit (0, maxSemis(), juce::roundToInt (rel * (float) maxSemis()));
    proc.arp.setStep (step, st);
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int i = stepAt ((float) e.x);
    dragStep = i;
    auto st = proc.arp.getStep (i);

    if (e.mods.isPopupMenu())
    {
        st.locked = ! st.locked;
        proc.arp.setStep (i, st);
        zone = Zone::none;
    }
    else if ((float) e.y < pitchTop())
    {
        st.vel = st.vel >= 100 ? 90 : 115;
        proc.arp.setStep (i, st);
        zone = Zone::accent;
    }
    else if ((float) e.y >= probY() - 6.0f)
    {
        zone = Zone::prob;
        startProb = st.prob;
        startY = (float) e.y;
    }
    else
    {
        zone = Zone::pitch;
        setPitchFromY (i, (float) e.y);
    }
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (zone == Zone::pitch)
    {
        setPitchFromY (stepAt ((float) e.x), juce::jlimit (pitchTop(), pitchBottom(), (float) e.y));
    }
    else if (zone == Zone::prob && dragStep >= 0)
    {
        auto st = proc.arp.getStep (dragStep);
        st.prob = juce::jlimit (0, 100, startProb + juce::roundToInt (startY - (float) e.y));
        proc.arp.setStep (dragStep, st);
    }
    repaint();
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    if ((float) e.y >= pitchTop() && (float) e.y < probY() - 6.0f)
    {
        const int i = stepAt ((float) e.x);
        auto st = proc.arp.getStep (i);
        st.semis = 0;
        proc.arp.setStep (i, st);
        repaint();
    }
}

void StepGrid::paint (juce::Graphics& g)
{
    const int numSteps = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::arpSteps)->load());
    const int scale = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::arpScale)->load());
    const int current = proc.arp.currentStep.load (std::memory_order_relaxed);
    const int maxS = maxSemis();

    for (int i = 0; i < 16; ++i)
    {
        const auto st = proc.arp.getStep (i);
        const auto col = column (i);
        const bool active = i < numSteps;
        const float alpha = active ? 1.0f : 0.3f;

        // Hintergrund und Playhead
        g.setColour (bleepcol::bg.withAlpha (alpha));
        g.fillRoundedRectangle (col, 4.0f);
        if (i == current)
        {
            g.setColour (bleepcol::red.withAlpha (0.16f));
            g.fillRoundedRectangle (col, 4.0f);
            g.setColour (bleepcol::red);
            g.drawRoundedRectangle (col.reduced (0.5f), 4.0f, 1.0f);
        }
        if (st.locked)
        {
            g.setColour (bleepcol::gold.withAlpha (0.8f * alpha));
            g.drawRoundedRectangle (col.reduced (0.5f), 4.0f, 1.0f);
        }

        // Akzent-Punkt
        const float cx = col.getCentreX();
        const bool accent = st.vel >= 100;
        g.setColour ((accent ? bleepcol::red : bleepcol::line).withAlpha (alpha));
        g.fillEllipse (cx - 4.0f, 6.0f, 8.0f, 8.0f);

        // Tonhoehe
        const float top = pitchTop(), bottom = pitchBottom();
        const float h = juce::jmax (3.0f, (bottom - top) * (float) juce::jmin (st.semis, maxS) / (float) maxS);
        const auto bar = juce::Rectangle<float> (col.getX() + 3.0f, bottom - h, col.getWidth() - 6.0f, h);
        const auto barCol = st.prob >= 80 ? bleepcol::red : juce::Colour (0xff5e4e52);
        g.setColour (barCol.withAlpha (alpha));
        g.fillRoundedRectangle (bar, 2.0f);

        // Wahrscheinlichkeit
        const auto pr = juce::Rectangle<float> (col.getX() + 3.0f, probY(), col.getWidth() - 6.0f, 4.0f);
        g.setColour (bleepcol::line.withAlpha (alpha));
        g.fillRoundedRectangle (pr, 2.0f);
        g.setColour (bleepcol::inkDim.withAlpha (alpha));
        g.fillRoundedRectangle (pr.withWidth (pr.getWidth() * (float) st.prob / 100.0f), 2.0f);

        // Intervall unter dem Step (auf die Tonleiter gerundet)
        const int snapped = bleep::snapToScale (juce::jmin (st.semis, maxS), scale);
        g.setFont (bb::fonts::mono (10.0f));
        g.setColour (bleepcol::inkDim.withAlpha (alpha));
        g.drawText (snapped == 0 ? juce::String ("0") : "+" + juce::String (snapped),
                    juce::Rectangle<float> (col.getX(), (float) getHeight() - 20.0f, col.getWidth(), 16.0f), juce::Justification::centred);
    }
}
