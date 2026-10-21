#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Etappe 3: ein eingefrorener Bleep (Slot) und die Sampler-Stimme, die ihn per Repitch spielt.

namespace bleep
{

// Ein gerenderter Bleep. Referenzgezaehlt: der Audio-Thread haelt nur Kopien des Zeigers,
// geloescht wird ausschliesslich auf dem Message-Thread (siehe BleepProcessor::purgeSlots).
struct SlotBuffer : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SlotBuffer>;

    juce::AudioBuffer<float> audio;   // mono
    double sampleRate = 48000.0;
    int rootNote = 48;
};

// 4-Punkt-Hermite-Interpolation, damit Repitch nach unten nicht matscht
inline float hermite (const float* d, int n, double pos)
{
    const int i = (int) pos;
    const float t = (float) (pos - (double) i);
    auto at = [d, n] (int k) { return d[juce::jlimit (0, n - 1, k)]; };
    const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

struct SlotSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

static constexpr int kNumSlots = 4;
using SlotArray = std::array<SlotBuffer::Ptr, kNumSlots>;

// Spielt einen Slot als One-Shot. Welcher Slot, steht im MIDI-Kanal der Note (Kanal 1 = Slot 1 ...).
// Repitch: hoehere Noten laufen schneller und werden kuerzer.
// Wird eine Stimme gestohlen oder abgebrochen, blendet der alte Klang 5 ms lang aus.
class SamplerVoice : public juce::SynthesiserVoice
{
public:
    SamplerVoice (const SlotArray& slotSources, const std::array<float, kNumSlots>& slotLevels)
        : sources (slotSources), levels (slotLevels) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SlotSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        moveToTail();
        int idx = 0;
        for (int ch = 1; ch <= kNumSlots; ++ch)
            if (isPlayingChannel (ch))
                idx = ch - 1;
        buf = sources[(size_t) idx];
        if (buf == nullptr || buf->audio.getNumSamples() < 4)
        {
            buf = nullptr;
            clearCurrentNote();
            return;
        }
        speed = std::exp2 ((midiNote - buf->rootNote) / 12.0) * buf->sampleRate / getSampleRate();
        pos = 0.0;
        gain = velocity * 0.25f * levels[(size_t) idx];   // gleicher Grundpegel wie die Live-Stimmen
        fadeLen = juce::jmax (1, (int) (0.005 * getSampleRate()));
        fadeIn = juce::jmax (1, (int) (0.001 * getSampleRate()));
        fadeInPos = 0;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
            return;   // One-Shot: Note-Off laesst den Bleep ausklingen
        moveToTail();
        clearCurrentNote();
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (buf == nullptr && tail == nullptr)
            return;

        const int numCh = out.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;

            if (buf != nullptr)
            {
                const int n = buf->audio.getNumSamples();
                float ramp = 1.0f;
                if (fadeInPos < fadeIn)
                    ramp = (float) fadeInPos++ / (float) fadeIn;
                s += hermite (buf->audio.getReadPointer (0), n, pos) * gain * ramp;
                pos += speed;
                if (pos >= (double) (n - 1))
                {
                    buf = nullptr;
                    clearCurrentNote();
                }
            }

            if (tail != nullptr)
            {
                const int n = tail->audio.getNumSamples();
                s += hermite (tail->audio.getReadPointer (0), n, tailPos) * tailGain * ((float) tailLeft / (float) fadeLen);
                tailPos += tailSpeed;
                if (--tailLeft <= 0 || tailPos >= (double) (n - 1))
                    tail = nullptr;
            }

            for (int ch = 0; ch < numCh; ++ch)
                out.addSample (ch, startSample + i, s);

            if (buf == nullptr && tail == nullptr)
                break;
        }
    }

private:
    void moveToTail()
    {
        if (buf == nullptr)
            return;
        tail = buf;
        tailPos = pos;
        tailSpeed = speed;
        tailGain = gain;
        tailLeft = fadeLen;
        buf = nullptr;
    }

    const SlotArray& sources;
    const std::array<float, kNumSlots>& levels;
    SlotBuffer::Ptr buf, tail;
    double pos = 0.0, speed = 1.0, tailPos = 0.0, tailSpeed = 1.0;
    float gain = 1.0f, tailGain = 0.0f;
    int fadeLen = 240, tailLeft = 0, fadeIn = 48, fadeInPos = 0;
};

} // namespace bleep
