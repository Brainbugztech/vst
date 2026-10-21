juce_add_plugin(BrainbugzBugFuzz
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.bugfuzz
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bfuz
    FORMATS VST3
    PRODUCT_NAME "Brainbugz BugFuzz"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BrainbugzBugFuzz PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BandView.cpp)

target_compile_definitions(BrainbugzBugFuzz PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzBugFuzz
    PRIVATE
        BrainBugzShared
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
