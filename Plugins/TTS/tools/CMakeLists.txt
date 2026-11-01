# Engine-Test für Linux (nicht Teil des Plugin-Builds):
#   cmake -S Plugins/TTS/tools -B build-tts-test -DJUCE_DIR=<juce> -DORT_DIR=<onnxruntime-linux>
cmake_minimum_required(VERSION 3.22)
project(TtsEngineTest VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
juce_add_console_app(tts_engine_test)
target_sources(tts_engine_test PRIVATE
    engine_test.cpp
    ../Source/engine/Phonemizer.cpp
    ../Source/engine/KokoroEngine.cpp)
target_include_directories(tts_engine_test PRIVATE ../Source ../Source/engine ${ORT_DIR}/include)
target_compile_definitions(tts_engine_test PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(tts_engine_test PRIVATE juce::juce_core juce::juce_audio_formats juce::juce_dsp
    juce::juce_recommended_config_flags)
