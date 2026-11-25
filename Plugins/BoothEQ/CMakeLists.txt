juce_add_plugin(BrainbugzBoothEQ
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.boothEq
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bbeq
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Booth EQ"
    VERSION 1.0.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(BoothEQAssets
    HEADER_NAME BoothEQAssets.h
    NAMESPACE BoothEQAssets
    SOURCES Assets/bb_logo.svg)
set_target_properties(BoothEQAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzBoothEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzBoothEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzBoothEQ
    PRIVATE
        BrainBugzShared
        BoothEQAssets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
