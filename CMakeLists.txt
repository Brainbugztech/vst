cmake_minimum_required(VERSION 3.22)

project(BrainBugz VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statische MSVC-Runtime: das Plugin braucht keine extra VC++-DLLs auf dem Zielrechner
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: lokaler Pfad (-DJUCE_PATH=...) oder automatisch von GitHub holen
set(JUCE_PATH "" CACHE PATH "Pfad zu einem lokalen JUCE-Checkout (leer = automatisch herunterladen)")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

add_subdirectory(Shared)
add_subdirectory(Plugins/Haas)
add_subdirectory(Plugins/BugFuzz)
# Plugins/Phazer: Engine in Arbeit, wird noch nicht gebaut
