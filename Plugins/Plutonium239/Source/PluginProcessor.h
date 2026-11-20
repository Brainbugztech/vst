#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "SynthEngine.h"
#include "Presets.h"

class PlutoProcessor : public juce::AudioProcessor
{
public:
    PlutoProcessor();
    ~PlutoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Plutonium239"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return (int) pluto::factoryPresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<pluto::WavetableBank> bank;
    pluto::SynthEngine engine;
    std::atomic<int> presetChanged { 0 };

private:
    int currentPreset = 0;

    struct OscPtrs { std::atomic<float> *wave, *wt, *oct, *semi, *cent, *pwm, *fine, *coarse, *a, *d, *s, *r, *lvl, *key, *sync,
                                        *dest = nullptr, *fmtype = nullptr, *fmamt = nullptr, *fmfine = nullptr, *fb = nullptr, *fdest = nullptr; };
    struct FiltPtrs { std::atomic<float> *on, *type, *cut, *res, *mix, *drive, *pan, *xpan, *lvl; };
    struct LfoPtrs { std::atomic<float> *shape, *rate, *sync, *div, *retrig, *tA, *aA, *tB, *aB; };
    OscPtrs op[pluto::numOsc];
    FiltPtrs fp[pluto::numFilt];
    LfoPtrs lp[pluto::numLfo];
    std::atomic<float> *mTune, *mOct, *mSemi, *mCent, *mFine, *mCoarse, *mVol, *mVoices, *mGlide, *mBend, *mVel;

    void buildPatch (pluto::Patch& p) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlutoProcessor)
};
