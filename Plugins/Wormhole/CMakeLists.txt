juce_add_plugin(BrainbugzWormhole
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.wormhole
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bwrm
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Wormhole"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Schriften (Inter, OFL) und weitere Grafiken
file(GLOB WORMHOLE_ASSET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.ttf")
juce_add_binary_data(WormholeAssets
    HEADER_NAME WormholeAssets.h
    NAMESPACE WormholeAssets
    SOURCES ${WORMHOLE_ASSET_FILES})
set_target_properties(WormholeAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Eigene Stretch-Engine + PFFFT (BSD-artige Lizenz, siehe Engine/pffft/LICENSE.txt)
add_library(WormholeFFT STATIC
    Engine/pffft/pffft.c
    Engine/pffft/pffft_common.c)
target_include_directories(WormholeFFT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Engine ${CMAKE_CURRENT_SOURCE_DIR}/Engine/pffft)
target_compile_definitions(WormholeFFT PRIVATE _USE_MATH_DEFINES PUBLIC PFFFT_STATIC_DEFINE)
set_target_properties(WormholeFFT PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(WormholeFFT PRIVATE /W0)
endif()

target_sources(BrainbugzWormhole PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Worker.cpp)

target_include_directories(BrainbugzWormhole PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine)

target_compile_definitions(BrainbugzWormhole PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzWormhole
    PRIVATE
        WormholeAssets
        WormholeFFT
        juce::juce_audio_processors
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
