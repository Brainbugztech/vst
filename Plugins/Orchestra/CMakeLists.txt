juce_add_plugin(BrainbugzOrchestra
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.orchestra
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Orch
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Orchestra"
    VERSION 0.1.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Orchester-Samples (VSCO 2 CE, CC0) als OGG direkt ins Plugin einbauen
file(GLOB ORCH_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Samples/*.ogg")
juce_add_binary_data(OrchestraSamples
    HEADER_NAME OrchestraSamples.h
    NAMESPACE OrchSamples
    SOURCES ${ORCH_SAMPLES})
set_target_properties(OrchestraSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzOrchestra PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzOrchestra PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzOrchestra
    PRIVATE
        BrainBugzShared
        OrchestraSamples
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
