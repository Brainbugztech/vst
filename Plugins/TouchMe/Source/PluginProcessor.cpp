#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace swarm;

TouchMeProcessor::TouchMeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TouchMe", createLayout())
{
    pX = apvts.getRawParameterValue ("x");
    pY = apvts.getRawParameterValue ("y");
    pTouch = apvts.getRawParameterValue ("touch");
    pHold = apvts.getRawParameterValue ("hold");
    pMix = apvts.getRawParameterValue ("dryWet");
    pProgram = apvts.getRawParameterValue ("program");
    pFx = apvts.getRawParameterValue ("effectMode");
}

juce::AudioProcessorValueTreeState::ParameterLayout TouchMeProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& first = presets().front();
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "x", 1 }, "Pad X", NormalisableRange<float> (0.0f, 1.0f), first.px));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "y", 1 }, "Pad Y", NormalisableRange<float> (0.0f, 1.0f), first.py));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "touch", 1 }, "Touch", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dryWet", 1 }, "Dry/Wet", NormalisableRange<float> (0.0f, 1.0f), first.mix,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "program", 1 }, "Preset", 0, numPresets() - 1, 0,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
        {
            return String (v + 1).paddedLeft ('0', 3) + " " + presets()[(size_t) jlimit (0, numPresets() - 1, v)].name;
        })));
    StringArray fx;
    for (int c = 0; c < C_NUM; ++c) fx.add (catName (c));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "effectMode", 1 }, "FX Engine", fx, 0));
    return { p.begin(), p.end() };
}

bool TouchMeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void TouchMeProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& b : bank) b.prepare (sr);
    wave.prepare (sr);
    currentProgram = -1;
    env = 0; wasTouched = false; ctrlCounter = 0;
    xs = pX->load(); ys = pY->load();
    mixSm = pMix->load();
}

void TouchMeProcessor::loadProgram (int idx)
{
    idx = juce::jlimit (0, numPresets() - 1, idx);
    const auto& pr = presets()[(size_t) idx];
    for (int s = 0; s < kSlots; ++s)
    {
        slot[s] = pr.s[s];
        algo[s] = bank[s].get (pr.s[s].algo);
        if (algo[s] != nullptr) algo[s]->reset();
        lfoPhase[s] = 0.0;
    }
    currentProgram = idx;
    activeProgram.store (idx);
}

void TouchMeProcessor::setParam (juce::RangedAudioParameter* p, float plainValue)
{
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    p->endChangeGesture();
}

void TouchMeProcessor::selectProgram (int idx, bool applyPadAndMix)
{
    idx = juce::jlimit (0, numPresets() - 1, idx);
    const auto& pr = presets()[(size_t) idx];
    setParam (apvts.getParameter ("program"), (float) idx);
    setParam (apvts.getParameter ("effectMode"), (float) pr.cat);
    if (applyPadAndMix)
    {
        setParam (apvts.getParameter ("x"), pr.px);
        setParam (apvts.getParameter ("y"), pr.py);
        setParam (apvts.getParameter ("dryWet"), pr.mix);
    }
}

void TouchMeProcessor::setCurrentProgram (int index) { selectProgram (index, true); }

const juce::String TouchMeProcessor::getProgramName (int index)
{
    return presets()[(size_t) juce::jlimit (0, numPresets() - 1, index)].name;
}

void TouchMeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    // Preset- und FX-Tasten-Parameter auswerten: was zuletzt geändert wurde, gewinnt
    const int progParam = (int) pProgram->load();
    const int fxParam = (int) pFx->load();
    int want = currentProgram < 0 ? progParam : currentProgram;
    if (progParam != lastProgParam) { want = progParam; lastProgParam = progParam; lastFxParam = fxParam; }
    else if (fxParam != lastFxParam)
    {
        lastFxParam = fxParam;
        if (presets()[(size_t) want].cat != fxParam) want = firstOfCategory (fxParam);
    }
    if (want != currentProgram) loadProgram (want);

    Ctx ctx; ctx.sr = sr;
    double ppqStart = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) ctx.bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppqStart = *q; playing = pos->getIsPlaying(); }
        }

    const bool touched = pTouch->load() > 0.5f || pHold->load() > 0.5f;
    const float tx = pX->load(), ty = pY->load();
    const float mixT = pMix->load();

    const float atk = 1.0f - std::exp (-1.0f / (0.006f * (float) sr));
    const float rel = 1.0f - std::exp (-1.0f / (0.030f * (float) sr));
    const float xyK = 1.0f - std::exp (-32.0f / (0.012f * (float) sr)); // pro Kontrollschritt
    const float gK = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        if (ctrlCounter == 0)
        {
            xs += (tx - xs) * xyK; ys += (ty - ys) * xyK;
            ctx.touched = touched;
            ctx.onset = touched && !wasTouched;
            wasTouched = touched;
            ctx.ppqValid = playing;
            ctx.ppq = ppqStart + (double) i / sr * ctx.bpm / 60.0;
            for (int s = 0; s < kSlots; ++s)
                if (algo[s] != nullptr)
                {
                    const auto& d = slot[s];
                    const float px = d.swap ? ys : xs, py = d.swap ? xs : ys;
                    float ax = lerp (d.x0, d.x1, px), ay = lerp (d.y0, d.y1, py);
                    if (d.lfoDiv >= 0 && d.lfoDepth > 0.0f)
                    {
                        // eigener LFO pro Effekt, im Songtempo (läuft frei, wenn der Host steht)
                        const float beats = kDivBeats[juce::jlimit (0, kNumDivs - 1, d.lfoDiv)] * (d.lfoShape == 4 ? 1.0f : 2.0f);
                        const double prev = lfoPhase[s];
                        if (playing) lfoPhase[s] = std::fmod (ctx.ppq / beats, 1.0);
                        else { lfoPhase[s] += 32.0 / beatsToSamples (beats, ctx); lfoPhase[s] -= std::floor (lfoPhase[s]); }
                        if (lfoPhase[s] < prev) lfoHold[s] = lfoRnd.nextFloat();
                        const float v = d.lfoShape == 4 ? lfoHold[s] : lfoShape ((float) lfoPhase[s], d.lfoShape);
                        const float off = (v - 0.5f) * 2.0f * d.lfoDepth;
                        if (d.lfoOnY) ay = clamp01 (ay + off); else ax = clamp01 (ax + off);
                    }
                    algo[s]->control (ax, ay, d.p, ctx);
                }
        }
        ctrlCounter = (ctrlCounter + 1) & 31;

        env += ((touched ? 1.0f : 0.0f) - env) * (touched ? atk : rel);
        mixSm += (mixT - mixSm) * gK;

        const float dryL = L[i], dryR = R[i];
        wave.push ((dryL + dryR) * 0.5f);

        float l = dryL, r = dryR;
        for (int s = 0; s < kSlots; ++s)
        {
            if (algo[s] == nullptr) continue;
            if (isSendAlgo (slot[s].algo) || slot[s].forceSend)
            {
                // Send: Eingang wird abgeschaltet, die Fahne läuft weiter
                float wl = l * env, wr = r * env;
                algo[s]->tick (wl, wr);
                l += wl; r += wr;
            }
            else
            {
                float wl = l, wr = r;
                algo[s]->tick (wl, wr);
                l += (wl - l) * env; r += (wr - r) * env;
            }
        }

        l = dryL + (l - dryL) * mixSm;
        r = dryR + (r - dryR) * mixSm;

        if (! std::isfinite (l) || ! std::isfinite (r))
        {
            l = r = 0.0f;
            for (auto* a : algo) if (a != nullptr) a->reset();
        }
        L[i] = juce::jlimit (-4.0f, 4.0f, l);
        R[i] = juce::jlimit (-4.0f, 4.0f, r);
    }

    envMeter.store (env);
}

void TouchMeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void TouchMeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TouchMeProcessor(); }
