juce_add_plugin(BrainbugzTTS
    COMPANY_NAME "Brain Bugz"
    BUNDLE_ID com.brainbugz.tts
    PLUGIN_MANUFACTURER_CODE Bbgz
    PLUGIN_CODE Btts
    FORMATS VST3
    PRODUCT_NAME "Brainbugz TTS"
    VERSION 0.1.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Grafiken + Aussprache-Lexika (misaki, Apache-2.0) direkt ins Plugin
juce_add_binary_data(TTSAssets
    HEADER_NAME TTSAssets.h
    NAMESPACE TTSAssets
    SOURCES
        Assets/tts_bg.jpg
        Assets/tts_bugglow.png
        Assets/tts_knob_big.png
        Assets/tts_knob_small.png
        Assets/lexicon_us.txt.gz
        Assets/lexicon_gb.txt.gz)
set_target_properties(TTSAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ONNX Runtime (MIT): nur die Header zum Bauen; die DLL kommt neben das Plugin in die VST3
# und wird erst geladen, wenn die Stimme gebraucht wird
include(FetchContent)
if(WIN32)
    FetchContent_Declare(onnxruntime
        URL https://github.com/microsoft/onnxruntime/releases/download/v1.22.0/onnxruntime-win-x64-1.22.0.zip
        URL_HASH SHA256=174c616efc0271194488642a72f1a514e01487da4dfe84c49296d66e40ebe0da
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
else()
    FetchContent_Declare(onnxruntime
        URL https://github.com/microsoft/onnxruntime/releases/download/v1.22.0/onnxruntime-linux-x64-1.22.0.tgz
        URL_HASH SHA256=8344d55f93d5bc5021ce342db50f62079daf39aaafb5d311a451846228be49b3
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()
FetchContent_MakeAvailable(onnxruntime)

target_sources(BrainbugzTTS PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/engine/Phonemizer.cpp
    Source/engine/KokoroEngine.cpp)

target_include_directories(BrainbugzTTS PRIVATE Source)
target_include_directories(BrainbugzTTS SYSTEM PRIVATE ${onnxruntime_SOURCE_DIR}/include)

target_compile_definitions(BrainbugzTTS PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BrainbugzTTS
    PRIVATE
        BrainBugzShared
        TTSAssets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    # Phonem-Tabellen enthalten Unicode
    target_compile_options(BrainbugzTTS PRIVATE /utf-8)
endif()

# Runtime-DLL unter eigenem Namen in die VST3 legen (Windows hat evtl. eine alte onnxruntime.dll in System32)
if(WIN32)
    add_custom_command(TARGET BrainbugzTTS_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:BrainbugzTTS_VST3>/bbtts_onnxruntime.dll"
        COMMENT "ONNX Runtime in die VST3 kopieren")
endif()
