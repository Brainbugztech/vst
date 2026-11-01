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

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter, overPainter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
        void paintOverChildren (juce::Graphics& g) override { if (overPainter) overPainter (g); }
    };

    // Wellenform + Download-/Status-Anzeige (zieht die WAV per Drag & Drop in die DAW)
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

    // Einstellungen (Zahnrad)
    struct Settings : public juce::Component
    {
        explicit Settings (TtsEditor& e);
        void paint (juce::Graphics&) override;
        void resized() override;
        TtsEditor& ed;
        juce::ComboBox blendVoice;
        juce::Slider blend { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
        juce::TextButton folder { "OPEN VOICE FOLDER" }, close { "CLOSE" };
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void paintOver (juce::Graphics&);
    void paintValueBox (juce::Graphics&, juce::Rectangle<int>, const juce::String&, float size, float alpha);
    void fillVoices();
    void syncFromProcessor();
    void updateStatusButtons();
    void saveWav();
    juce::File writeDragFile();

    TtsProcessor& proc;
    ttsui::Look lnf;
    Content content;
    juce::Image bg, bugGlow, capBig, capSmall;
    double timeSec = 0.0, lastMs = 0.0;
    int lastAudioVersion = -1;
    std::vector<float> peaks;
    juce::String lastStatus;

    juce::ComboBox voiceBox, accentBox;
    ttsui::GlowButton gearBtn { "settings", juce::Colour (0xff2f9bff), 25.0f };
    juce::TextEditor textBox;
    ttsui::GlowButton scrollUp { "up", ttsui::col::purple }, scrollDown { "down", ttsui::col::purple };
    ttsui::GlowButton playBtn { "play", ttsui::col::cyan }, stopBtn { "stop", ttsui::col::purple },
                      genBtn { "generate", ttsui::col::cyan }, saveBtn { "save", ttsui::col::cyan2 };
    ttsui::GlowButton fxBtn { "fx", ttsui::col::cyan, 24.0f };

    std::unique_ptr<ttsui::Knob> pitchK, speedK, volumeK, formantK, reverbK, delayK, eqK;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> pitchA, speedA, volumeA, formantA, reverbA, delayA, eqA;
    std::unique_ptr<juce::ParameterAttachment> fxAtt;
    bool fxOn = true;

    WaveView wave { *this };
    juce::TextButton downloadBtn, cancelBtn { "CANCEL" }, retryBtn { "RETRY" }, folderBtn { "OPEN FOLDER" };
    Settings settings { *this };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String toast;
    double toastUntil = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TtsEditor)
};
