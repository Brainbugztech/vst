// Kurzer Film der echten Oberfläche (Testwerkzeug)
#include "PluginProcessor.h"
#include "PluginEditor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    WormholeProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    proc.apvts.getParameter ("mode")->setValueNotifyingHost (1.0f);
    proc.worker.loadFile (juce::File (argv[2]));
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1440, 900);
    for (int i = 0; i < 40; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (25);
    juce::File dir (argv[1]); dir.createDirectory();
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
    int frame = 0;
    auto setP = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    auto step = [&] (int frames)
    {
        for (int f = 0; f < frames; ++f)
        {
            // 1/25 s Audio + Nachrichten
            for (int k = 0; k < 3; ++k) { b.clear(); proc.processBlock (b, m); }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (36);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File out = dir.getChildFile (juce::String (frame++).paddedLeft ('0', 5) + ".png");
            juce::FileOutputStream os (out); os.setPosition (0); os.truncate();
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
    };
    step (25);                                                    // Ruhe
    for (int i = 0; i <= 30; ++i) { setP ("tempo", 100.0f - 60.0f * i / 30.0f); step (1); }   // Tempo drehen
    step (12);
    for (int i = 0; i <= 25; ++i) { setP ("pitch", (float) juce::roundToInt (7.0f * i / 25.0f)); step (1); } // Pitch hoch
    step (12);
    proc.preview.play (proc.worker.getClip(), proc.currentRenderSettings(), 0.2);  // Vorhören
    step (60);
    proc.preview.stop(); step (10);
    proc.worker.render (proc.currentRenderSettings());              // Rendern + Lichtimpuls
    for (int i = 0; i < 90 && (proc.worker.getStatus() == wh::Worker::Status::rendering || i < 5); ++i) step (1);
    step (35);
    return 0;
}
