cmake_minimum_required(VERSION 3.16)

project(audio_analyser
    VERSION 0.1.0
    LANGUAGES C
)


# ============================================================================
# Project options
# ============================================================================

option(ANALYSER_BUILD_TESTS
    "Build audio-analyser tests"
    OFF
)


# ============================================================================
# C standard
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


# ============================================================================
# Third-party dependencies
# ============================================================================
#
# The dependencies are vendored in the repository but are not part of the
# public installation of audio-analyser.
#
# Their own CMake projects are used to build them.
# ============================================================================

# ----------------------------------------------------------------------------
# libsndfile
# ----------------------------------------------------------------------------

set(BUILD_PROGRAMS OFF CACHE BOOL
    "Build libsndfile programs"
    FORCE
)

set(BUILD_EXAMPLES OFF CACHE BOOL
    "Build libsndfile examples"
    FORCE
)

set(BUILD_TESTING OFF CACHE BOOL
    "Build libsndfile tests"
    FORCE
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third-party/libsndfile
    ${CMAKE_CURRENT_BINARY_DIR}/third-party/libsndfile
)


# ----------------------------------------------------------------------------
# aubio
# ----------------------------------------------------------------------------

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third-party/aubio
    ${CMAKE_CURRENT_BINARY_DIR}/third-party/aubio
)


# ============================================================================
# audio-analyser library
# ============================================================================

add_library(analyser

    src/analyser.c
    src/audio.c
    src/onset.c
    src/aggregate.c
    src/analyser_json.c

)


# ============================================================================
# Include directories
# ============================================================================

# Public headers.
target_include_directories(analyser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Private project headers.
target_include_directories(analyser
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)


# ============================================================================
# Dependencies
# ============================================================================

target_link_libraries(analyser
    PRIVATE
        SndFile::sndfile
        aubio
)


# ============================================================================
# Compiler warnings
# ============================================================================

if(MSVC)

    target_compile_options(analyser
        PRIVATE
            /W4
    )

else()

    target_compile_options(analyser
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

endif()


# ============================================================================
# Installation
# ============================================================================
#
# Only audio-analyser itself is installed.
#
# aubio and libsndfile are intentionally NOT installed by this project.
# ============================================================================

include(GNUInstallDirs)

install(
    TARGETS analyser
    EXPORT analyserTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)


# ============================================================================
# Tests
# ============================================================================

if(ANALYSER_BUILD_TESTS)

    enable_testing()

    add_subdirectory(tests)

endif()
