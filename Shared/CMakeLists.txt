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
