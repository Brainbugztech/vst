#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <iterator>
#include "PluginProcessor.h"
#include "ui/TtsWidgets.h"

class TtsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TtsEditor (TtsProcessor&);
    ~TtsEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int baseW = 1536, baseH = 1024;

    TtsProcessor& proc;
    ttsui::Look lnf;
    juce::Image bg, bugGlow, capBig, capSmall;
    double timeSec = 0.0;

private:
    // Fläche mit fester Größe, wird beim Skalieren des Fensters mitgezoomt
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter, overPainter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
        void paintOverChildren (juce::Graphics& g) override { if (overPainter) overPainter (g); }
    };

    // Seiten liegen deckungsgleich über der ganzen Fläche, nur ihre Bedienelemente fangen Klicks
    struct Page : public juce::Component
    {
        Page() { setInterceptsMouseClicks (false, true); }
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    struct WaveView : public juce::Component
    {
        explicit WaveView (TtsEditor& e) : ed (e) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { dragged = false; }
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool hitTest (int x, int y) override;
        TtsEditor& ed;
        bool dragged = false;
    };

    struct GrainCloud : public juce::Component
    {
        explicit GrainCloud (TtsEditor& e) : ed (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        TtsEditor& ed;
    };

    struct EqGraph : public juce::Component
    {
        explicit EqGraph (TtsEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        float xForFreq (float f) const;
        float freqForX (float x) const;
        float yForDb (float db) const;
        float dbForY (float y) const;
        juce::Point<float> nodePos (int band) const;
        int hitNode (juce::Point<float>) const;
        void setParam (const juce::String& id, float value);
        TtsEditor& ed;
        int dragBand = -1;
    };

    struct Settings : public juce::Component
    {
        explicit Settings (TtsEditor& e);
        void paint (juce::Graphics&) override;
        void resized() override;
        TtsEditor& ed;
        juce::TextButton folder { "OPEN VOICE FOLDER" }, close { "CLOSE" };
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void paintOver (juce::Graphics&);
    void paintMainPage (juce::Graphics&);
    void paintGrainPage (juce::Graphics&);
    void paintFxPage (juce::Graphics&);
    void paintValueBox (juce::Graphics&, juce::Rectangle<int>, const juce::String&, float size, float alpha);
    void fillVoices();
    void syncFromProcessor();
    void updateStatusButtons();
    void showTab (int t);
    void selectBand (int b);
    void updateSectionAlpha();
    void saveWav();
    juce::File writeDragFile();
    void addParamKnob (Page&, std::unique_ptr<ttsui::ParamKnob>&, const char* id, const juce::String& label, juce::Rectangle<int>,
                       bool bipolar = false, float capR = 22.0f);
    float param (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    Content content;
    Page mainPage, grainPage, fxPage;
    int lastAudioVersion = -1;
    std::vector<float> peaks;
    int tab = 0, selBand = 0;

    // ---- immer sichtbar
    juce::ComboBox voiceBox, accentBox;
    ttsui::GlowButton gearBtn { "settings", juce::Colour (0xff2f9bff), 25.0f };
    juce::TextEditor textBox;
    ttsui::GlowButton scrollUp { "up", ttsui::col::purple }, scrollDown { "down", ttsui::col::purple };
    ttsui::GlowButton playBtn { "play", ttsui::col::cyan }, stopBtn { "stop", ttsui::col::purple },
                      genBtn { "generate", ttsui::col::cyan }, saveBtn { "save", ttsui::col::cyan2 };
    juce::TextButton tabBtn[3];
    WaveView wave { *this };
    juce::TextButton downloadBtn, cancelBtn { "CANCEL" }, retryBtn { "RETRY" }, folderBtn { "OPEN FOLDER" };
    Settings settings { *this };

    // ---- MAIN
    std::unique_ptr<ttsui::Knob> pitchK, speedK, volumeK, formantK;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> pitchA, speedA, volumeA, formantA;
    juce::ComboBox blendVoiceBox;
    std::unique_ptr<ttsui::Knob> blendK;

    // ---- GRAIN
    ttsui::PowerButton grainPower;
    juce::TextButton freezeBtn { "FREEZE" };
    std::unique_ptr<BA> grainPowerA, freezeA;
    std::unique_ptr<ttsui::ParamKnob> gSize, gFlux, gDensity, gSpray, gPitch, gWidth, gReverse, gMix;
    GrainCloud cloud { *this };

    // ---- FX
    ttsui::PowerButton eqPower, dlPower, rvPower, bandPower[3];
    std::unique_ptr<BA> eqPowerA, dlPowerA, rvPowerA, bandPowerA[3];
    juce::TextButton bandBtn[3];
    juce::ComboBox bandType[3];
    std::unique_ptr<CA> bandTypeA[3];
    std::unique_ptr<ttsui::ParamKnob> bandFreq[3], bandGain[3], bandQ[3];
    EqGraph eqGraph { *this };
    std::unique_ptr<ttsui::ParamKnob> dTimeSync, dTimeMs, dFeedback, dMix, dLow, dHigh;
    juce::TextButton syncBtn { "SYNC" }, pingBtn { "PING-PONG" };
    std::unique_ptr<BA> syncA, pingA;
    std::unique_ptr<ttsui::ParamKnob> rSize, rDamp, rMix, rPre, rWidth, rLow;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::File dragFile;
    juce::String dragKey;
    juce::String toast;
    double toastUntil = 0.0, lastMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TtsEditor)
};
