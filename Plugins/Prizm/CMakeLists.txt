juce_add_plugin(BrainbugzPrizm
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.prizm
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bprz
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Prizm"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Hintergrund-Textur (erzeugt mit tools/make_backdrop.py)
file(GLOB PRIZM_ASSET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.png")
juce_add_binary_data(PrizmAssets
    HEADER_NAME PrizmAssets.h
    NAMESPACE PrizmAssets
    SOURCES ${PRIZM_ASSET_FILES})
set_target_properties(PrizmAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(PRIZM_SOURCES
    Source/FilterBankEngine.cpp
    Source/SpectralEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PitchMatrix.cpp)

target_sources(BrainbugzPrizm PRIVATE ${PRIZM_SOURCES})

target_compile_definitions(BrainbugzPrizm PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzPrizm
    PRIVATE
        BrainBugzShared
        PrizmAssets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline-Test (nicht Teil des Plugins): cmake -B build -DPRIZM_BUILD_TESTS=ON
option(PRIZM_BUILD_TESTS "Prizm: Offline-Render-Test bauen" OFF)
if(PRIZM_BUILD_TESTS)
    juce_add_console_app(prizm_render_test PRODUCT_NAME "prizm_render_test")
    target_sources(prizm_render_test PRIVATE tools/render_test.cpp ${PRIZM_SOURCES})
    target_compile_definitions(prizm_render_test PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 PRIZM_HEADLESS_TEST=1)
    target_link_libraries(prizm_render_test PRIVATE
        BrainBugzShared PrizmAssets
        juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
