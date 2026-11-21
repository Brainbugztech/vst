# Windows: VST3, Mac: VST3 + AU (für Ableton/Logic)
if(APPLE)
    set(TOUCHME_FORMATS VST3 AU)
else()
    set(TOUCHME_FORMATS VST3)
endif()

juce_add_plugin(BrainbugzTouchMe
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.touchme
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Btch
    FORMATS ${TOUCHME_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "BrainBugz Touch Me!"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Grafiken aus Benetts GUI-Kit (aufbereitet mit tools/make_assets.py) und Share Tech Mono (OFL)
file(GLOB TOUCHME_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.ttf")
juce_add_binary_data(TouchMeAssets
    HEADER_NAME TouchMeAssets.h
    NAMESPACE TouchMeAssets
    SOURCES ${TOUCHME_ASSETS})
set_target_properties(TouchMeAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzTouchMe PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BrainbugzTouchMe PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzTouchMe
    PRIVATE
        TouchMeAssets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
