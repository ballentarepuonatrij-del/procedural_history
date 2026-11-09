cmake_minimum_required(VERSION 3.21)
project(FantasyChronicle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(FantasyCore STATIC
    src/WorldTypes.cpp
    src/Names.cpp
    src/Geography.cpp
    src/World.cpp
    src/Simulation.cpp
)
target_include_directories(FantasyCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(FantasyCore PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(FantasyCore PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(FantasyCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_executable(FantasyChronicle WIN32 src/Win32App.cpp)
    target_link_libraries(FantasyChronicle PRIVATE FantasyCore comctl32 comdlg32 gdi32 user32)
    target_compile_definitions(FantasyChronicle PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(FantasyChronicle PRIVATE /W4 /utf-8 /EHsc)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(FantasySmoke tests/smoke.cpp)
    target_link_libraries(FantasySmoke PRIVATE FantasyCore)
    add_test(NAME world_generation_smoke COMMAND FantasySmoke)
endif()
