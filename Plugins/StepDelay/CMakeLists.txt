juce_add_plugin(BrainbugzStepDelay
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.stepdelay
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bstp
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Step Delay"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Hintergrund nach Benetts Vorlage + Schrift (Rajdhani kommt im Build-Workflow dazu)
file(GLOB SD_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.ttf")
juce_add_binary_data(StepDelayAssets
    HEADER_NAME StepDelayAssets.h
    NAMESPACE SDAssets
    SOURCES ${SD_ASSETS})
set_target_properties(StepDelayAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzStepDelay PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzStepDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzStepDelay
    PRIVATE
        BrainBugzShared
        StepDelayAssets
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
