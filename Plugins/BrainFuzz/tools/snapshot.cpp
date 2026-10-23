// Brainfuzz – rendert die Oberflaeche mit laufendem Testaudio in ein PNG (nur fuer Entwicklung)
// Wird nur mit -DBRAINFUZZ_SNAPSHOT=ON gebaut: brainfuzz_snapshot <out.png> [preset-index] [scale] [fx-tab]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String out = argc > 1 ? argv[1] : "brainfuzz.png";
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

    BrainfuzzProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    proc.loadPreset (preset);
    proc.uiScale = scale;
    if (argc > 4) for (auto& t : proc.fxTab) t = juce::String (argv[4]).getIntValue();

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditorIfNeeded());
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double ph[4] {};
    juce::Random rnd (7);
    long n = 0;
    for (int frame = 0; frame < 90; ++frame)
    {
        for (int k = 0; k < 3; ++k)
        {
            for (int i = 0; i < 512; ++i, ++n)
            {
                const double t = n / 48000.0, beat = std::fmod (t, 60.0 / 145.0);
                const float kick = (float) (0.7 * std::exp (-beat * 9.0) * std::sin (6.2831853 * (50.0 * beat + 2.0 * (1.0 - std::exp (-beat * 30.0)))));
                ph[0] += 55.0 / 48000.0; ph[1] += 220.0 / 48000.0; ph[2] += 329.6 / 48000.0; ph[3] += 1318.5 / 48000.0;
                const float saw = (float) (std::fmod (ph[0], 1.0) * 2.0 - 1.0) * 0.25f;
                const float pad = (float) ((std::fmod (ph[1], 1.0) + std::fmod (ph[2], 1.0)) - 1.0) * 0.08f;
                const float hi = (float) std::sin (6.2831853 * ph[3]) * 0.04f;
                const float hat = (rnd.nextFloat() * 2.0f - 1.0f) * 0.12f * (float) std::exp (-std::fmod (t + 0.2, 60.0 / 145.0) * 40.0);
                buf.setSample (0, i, kick + saw + pad + hi + hat);
                buf.setSample (1, i, kick + saw + pad * 0.7f - hi + hat * 0.8f);
            }
            proc.processBlock (buf, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (out));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    ed.reset();
    std::printf ("geschrieben: %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
