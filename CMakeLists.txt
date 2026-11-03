cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(Template VERSION 1.0.0)

# Add all source files inside src (recursively)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Set up the mod binary
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

set(GEODE_DISABLE_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
add_subdirectory("$ENV{GEODE_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/geode")

# Force the standard Geode declarations into generated MSVC binding units
if (MSVC)
    get_filename_component(AP_GEODE_BINDINGS_HEADER
        "$ENV{GEODE_SDK}/loader/include/Geode/Bindings.hpp" ABSOLUTE)
    if (NOT EXISTS "${AP_GEODE_BINDINGS_HEADER}")
        message(FATAL_ERROR "Missing Geode header: ${AP_GEODE_BINDINGS_HEADER}")
    endif()
    target_compile_options(GeodeBindings PRIVATE "/FI${AP_GEODE_BINDINGS_HEADER}")
endif()

CPMAddPackage("gh:camila314/UIBuilder#2ab80ad")
target_link_libraries(${PROJECT_NAME} UIBuilder)
CPMAddPackage("gh:dankmeme01/cue#main")
target_link_libraries(${PROJECT_NAME} cue)

# Set up dependencies, resources, and link Geode.
setup_geode_mod(${PROJECT_NAME})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor)
endif()

if (MSVC)
    set_source_files_properties(
        src/ui/ghost_settings_popup.cpp
        src/ui/attempt_manager_transfers.cpp
        src/ui/tabbed_settings_popup.cpp
        src/ui/playback_mod_menu_popup.cpp
        PROPERTIES
            SKIP_PRECOMPILE_HEADERS ON
            COMPILE_OPTIONS "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/Od>"
    )
endif()
