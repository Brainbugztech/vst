#pragma once

#include "Widgets.h"
#include <juce_dsp/juce_dsp.h>

namespace bfz::ui
{

// Analyser: Live-Spektrum jedes Bandes in seiner Farbe, Weichen und Drive direkt im Display
//  - Grenzen ziehen = Weiche verschieben, Griff hoch/runter = Drive des Bandes
//  - Shift = fein, Doppelklick = Standard, Mausrad ueber dem Band = Drive
//  - Frequenz- oder Drive-Wert anklicken = eintippen
class Analyser : public juce::Component
{
public:
    explicit Analyser (BrainfuzzProcessor& p);

    void update();   // vom Editor-Timer
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    static constexpr int kOrder = 12, kSize = 1 << kOrder, kBins = kSize / 2;
    static constexpr float kMinF = 20.0f, kMaxDb = 6.0f, kMinDb = -84.0f, kMaxDrive = 36.0f;

    enum class HitKind { none, xover, drive, xLabel, dLabel };
    struct Hit { HitKind kind = HitKind::none; int index = -1; };

    juce::Rectangle<float> plotArea() const;
    float maxF() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float driveToY (float d) const;
    float yToDrive (float y) const;
    float bandLeftX (int b) const;
    float bandRightX (int b) const;
    juce::Rectangle<float> xLabelRect (int k) const;
    juce::Rectangle<float> driveLabelRect (int b) const;
    juce::Point<float> handlePos (int b) const;
    Hit hitTest (juce::Point<float> p) const;
    void computeSpectrum (const VizRing<BrainfuzzProcessor::kSpecRing>& ring, std::vector<float>& smoothDb);
    juce::Path spectrumPath (const std::vector<float>& db, bool closed) const;
    void beginEdit (int paramIndex);
    void endEdit();
    void editValue (int paramIndex, juce::Rectangle<float> where);

    BrainfuzzProcessor& proc;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftBuf;
    std::array<std::vector<float>, 4> bandDb;
    std::vector<float> outDb, inDb;
    Hit hover, active;
    int editingParam = -1;
    float dragStartValue = 0.0f;
    juce::Point<float> dragStart;
    std::unique_ptr<juce::TextEditor> editor;
    juce::Image background;
};

} // namespace bfz::ui
