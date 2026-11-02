juce_add_plugin(BrainbugzKickTransmuter
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.kicktransmuter
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Ktmt
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Kick Transmuter"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Grafiken nur für dieses Plugin (erzeugt mit tools/make_assets.py aus Benetts GUI-Bild)
juce_add_binary_data(KickTransmuterAssets
    HEADER_NAME KickTransmuterAssets.h
    NAMESPACE KTAssets
    SOURCES Assets/kick_bg.jpg Assets/kick_brain.png)
set_target_properties(KickTransmuterAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzKickTransmuter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Oversampling kommt aus Brainfuzz (dsp/Oversampler.h)
target_include_directories(BrainbugzKickTransmuter PRIVATE Source ../BrainFuzz/Source)

target_compile_definitions(BrainbugzKickTransmuter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzKickTransmuter
    PRIVATE
        BrainBugzShared
        KickTransmuterAssets
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
