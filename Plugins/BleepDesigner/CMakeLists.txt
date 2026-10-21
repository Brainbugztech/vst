juce_add_plugin(BrainbugzBleepDesigner
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.bleepdesigner
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bblp
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Bleep Designer"
    VERSION 0.1.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BrainbugzBleepDesigner PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzBleepDesigner PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzBleepDesigner
    PRIVATE
        BrainBugzShared
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
