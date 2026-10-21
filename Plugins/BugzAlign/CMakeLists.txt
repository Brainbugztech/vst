juce_add_plugin(BrainbugzBugzAlign
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.bugzalign
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Balg
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Bugz Align"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Tools
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BrainbugzBugzAlign PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AlignView.cpp)

target_compile_definitions(BrainbugzBugzAlign PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzBugzAlign
    PRIVATE
        BrainBugzShared
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
