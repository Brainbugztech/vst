juce_add_plugin(BrainbugzBraincut
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.braincut
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bcut
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Braincut"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Hintergrundbild nur für dieses Plugin (nicht in den gemeinsamen Assets)
juce_add_binary_data(BraincutAssets
    HEADER_NAME BraincutAssets.h
    NAMESPACE BCAssets
    SOURCES Assets/braincut_bg.jpg)
set_target_properties(BraincutAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzBraincut PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(BrainbugzBraincut PRIVATE Source)

target_compile_definitions(BrainbugzBraincut PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzBraincut
    PRIVATE
        BrainBugzShared
        BraincutAssets
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
