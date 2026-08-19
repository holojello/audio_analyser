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
    ON
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
# Dependencies are downloaded from their upstream repositories at fixed
# commits and patched locally before being added to the build.
#
# The dependency sources and build products live below the build directory
# and are not installed by audio-analyser.
# ============================================================================

include(FetchContent)


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

FetchContent_Declare(
    libsndfile
    URL
        https://github.com/libsndfile/libsndfile/archive/7ff854d1e0bd9a751a9ff52ed980c62afced91fe.tar.gz
    PATCH_COMMAND
        patch -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/libsndfile.patch
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(libsndfile)


# ----------------------------------------------------------------------------
# aubio
# ----------------------------------------------------------------------------

FetchContent_Declare(
    aubio
    URL
        https://github.com/aubio/aubio/archive/ad5cf975aed08cc4562dd008cf9f83b12b82ffb8.tar.gz
    PATCH_COMMAND
        patch -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/aubio.patch
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(aubio)


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

    add_executable(test_audio_mono
        tests/test_audio_mono.c
    )

    target_link_libraries(test_audio_mono
        PRIVATE
            analyser
    )

    target_include_directories(test_audio_mono
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(
        NAME audio_mono
        COMMAND test_audio_mono
    )

endif()
