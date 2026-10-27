// Offline-Snapshot der echten JUCE-Oberfläche (nur für Tests, nicht Teil des Plugins)
#include "PluginProcessor.h"
#include "PluginEditor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    WormholeProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    if (argc > 2 && juce::String (argv[2]) == "render") proc.apvts.getParameter ("mode")->setValueNotifyingHost (1.0f);
    if (argc > 3) { proc.worker.loadFile (juce::File (argv[3])); }
    for (int i = 4; i + 1 < argc; i += 2)
        if (auto* p = proc.apvts.getParameter (argv[i])) p->setValueNotifyingHost (p->convertTo0to1 (juce::String (argv[i + 1]).getFloatValue()));
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    // Worker arbeiten lassen und Timer laufen lassen
    for (int i = 0; i < 60; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    if (argc > 3 && juce::String (argv[argc - 1]) == "dorender")
    {
        proc.worker.render (proc.currentRenderSettings());
        for (int i = 0; i < 200 && proc.worker.getStatus() != wh::Worker::Status::idle; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        for (int i = 0; i < 20; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }
    ed->setSize (1920, 1200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argv[1]); out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
