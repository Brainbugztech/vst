juce_add_plugin(BrainbugzSnareHead
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.snarehead
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bsnh
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Snare Head"
    VERSION 0.2.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Logo und Drip-Schrift (Creepster, OFL – wird beim Build geladen)
file(GLOB SNAREHEAD_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.ttf")
juce_add_binary_data(SnareHeadAssets
    HEADER_NAME SnareHeadAssets.h
    NAMESPACE SnareHeadAssets
    SOURCES ${SNAREHEAD_ASSETS})
set_target_properties(SnareHeadAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzSnareHead PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzSnareHead PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzSnareHead
    PRIVATE
        BrainBugzShared
        SnareHeadAssets
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
