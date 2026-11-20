juce_add_plugin(Plutonium239
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.plutonium239
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Pl39
    FORMATS VST3
    PRODUCT_NAME "Plutonium239"
    VERSION 0.1.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Plutonium239 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Plutonium239 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Plutonium239
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
