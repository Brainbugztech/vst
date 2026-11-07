juce_add_plugin(BrainbugzClubEQ
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.clubeq
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bceq
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Club EQ"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(ClubEQAssets
    HEADER_NAME ClubEQAssets.h
    NAMESPACE ClubEQAssets
    SOURCES Assets/bb_logo.svg)
set_target_properties(ClubEQAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzClubEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzClubEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzClubEQ
    PRIVATE
        BrainBugzShared
        ClubEQAssets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
