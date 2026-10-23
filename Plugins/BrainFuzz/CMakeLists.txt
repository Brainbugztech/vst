juce_add_plugin(BrainbugzBrainfuzz
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.brainfuzz
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Bbfz
    FORMATS VST3
    PRODUCT_NAME "Brainbugz Brainfuzz"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Logo und Titel-Schrift (Audiowide, OFL – wird beim Build geladen)
file(GLOB BRAINFUZZ_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.ttf")
juce_add_binary_data(BrainfuzzAssets
    HEADER_NAME BrainfuzzAssets.h
    NAMESPACE BrainfuzzAssets
    SOURCES ${BRAINFUZZ_ASSETS})
set_target_properties(BrainfuzzAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BrainbugzBrainfuzz PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/Analyser.cpp
    Source/ui/BandModule.cpp
    Source/ui/SidePanels.cpp)

target_include_directories(BrainbugzBrainfuzz PRIVATE Source)

target_compile_definitions(BrainbugzBrainfuzz PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzBrainfuzz
    PRIVATE
        BrainBugzShared
        BrainfuzzAssets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Nur fuer die Entwicklung: Screenshot-Tool (cmake -DBRAINFUZZ_SNAPSHOT=ON)
option(BRAINFUZZ_SNAPSHOT "Screenshot-Tool fuer Brainfuzz bauen" OFF)
if(BRAINFUZZ_SNAPSHOT)
    juce_add_console_app(brainfuzz_snapshot PRODUCT_NAME "brainfuzz_snapshot")
    target_sources(brainfuzz_snapshot PRIVATE
        tools/snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/Analyser.cpp
        Source/ui/BandModule.cpp
        Source/ui/SidePanels.cpp)
    target_include_directories(brainfuzz_snapshot PRIVATE Source)
    target_compile_definitions(brainfuzz_snapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_VersionString="0.1.0"
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(brainfuzz_snapshot PRIVATE
        BrainBugzShared BrainfuzzAssets
        juce::juce_audio_processors juce::juce_dsp juce::juce_gui_extra
        juce::juce_recommended_config_flags)
endif()
