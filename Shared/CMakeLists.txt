# Gemeinsame Grafiken und Schriften für alle Brain-Bugz-Plugins
file(GLOB BB_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Assets/*.png"
    "${CMAKE_SOURCE_DIR}/Assets/*.ttf")

juce_add_binary_data(BrainBugzAssets
    HEADER_NAME BrainBugzAssets.h
    NAMESPACE BBAssets
    SOURCES ${BB_ASSETS})

set_target_properties(BrainBugzAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(BrainBugzShared INTERFACE)
target_include_directories(BrainBugzShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BrainBugzShared INTERFACE BrainBugzAssets)

# ---- Brain Bugz Center: Lizenzen + Auto-Updates (Modul in Shared/bb_center)
# Die Schluessel-Datei bb_center_keys.h liegt NICHT im Repo. Gesucht wird:
#   1. Shared/bb_center/bb_center_keys.h  (lokal oder im GitHub-Build aus dem Secret BB_CENTER_KEYS)
#   2. Ordner aus der Umgebungsvariable BB_CENTER_KEYS_DIR
#   3. G:/Claude Ordner/shared/bb_center  (Benetts PC)
# Ohne ausgefuellte Datei werden die Plugins ohne Lizenzsystem gebaut (laufen dann frei).
set(_bb_enabled OFF)
set(_bb_keys_dir "")
foreach(_dir "${CMAKE_CURRENT_SOURCE_DIR}/bb_center" "$ENV{BB_CENTER_KEYS_DIR}" "G:/Claude Ordner/shared/bb_center")
    if(NOT _bb_enabled AND NOT "${_dir}" STREQUAL "" AND EXISTS "${_dir}/bb_center_keys.h")
        file(READ "${_dir}/bb_center_keys.h" _bb_keys_text)
        if(_bb_keys_text MATCHES "BB_KEYS_CONFIGURED")
            set(_bb_enabled ON)
            set(_bb_keys_dir "${_dir}")
        endif()
    endif()
endforeach()

if(_bb_enabled)
    juce_add_module("${CMAKE_CURRENT_SOURCE_DIR}/bb_center")
    target_include_directories(bb_center INTERFACE "${_bb_keys_dir}")
    message(STATUS "Brain Bugz Center: MIT Lizenzsystem (Schluessel aus ${_bb_keys_dir})")
else()
    message(WARNING "Brain Bugz Center: bb_center_keys.h nicht ausgefuellt -> Plugins ohne Lizenzsystem")
endif()
set(BB_CENTER_ENABLED ${_bb_enabled} CACHE INTERNAL "Lizenzsystem verfuegbar")
