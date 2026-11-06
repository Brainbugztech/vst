juce_add_plugin(BrainbugzGainLock
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.gainlock
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bglk
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Gain Lock"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Tools
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(GainLockAssets
    HEADER_NAME GainLockAssets.h
    NAMESPACE GainLockAssets
    SOURCES Assets/gainlock_logo.png)
set_target_properties(GainLockAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzGainLock PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzGainLock PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzGainLock
    PRIVATE
        BrainBugzShared
        GainLockAssets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
