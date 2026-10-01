cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# PPF-42 DYNAMICS (Poopenfarten Pro) — CMake Multi-Platform & Multi-Target Build
# Formats: VST3, CLAP 1.0+, Standalone Executable (.exe)
# Headless Verification Suite: ppf42_headless_dsp_tests.exe
# ==============================================================================

project(PPF42_DYNAMICS_PROJECT VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and Warning Flags
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/utf-8 /W4 /O2 /fp:precise)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        add_compile_options(/arch:AVX2)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

include(FetchContent)

# ==============================================================================
# 1. Provision Microsoft.Web.WebView2 NuGet Package for Windows
# ==============================================================================
if(WIN32)
    set(WEBVIEW2_VERSION "1.0.2849.39")
    set(LOCAL_PARENT_PKG "${CMAKE_CURRENT_SOURCE_DIR}/../braun_as-42/build/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
    set(BINARY_PKG_DIR   "${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")

    if(EXISTS "${LOCAL_PARENT_PKG}/build/native/include/WebView2.h")
        message(STATUS "Using cached Microsoft.Web.WebView2 NuGet package from braun_as-42...")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../braun_as-42/build/packages" CACHE PATH "WebView2 location" FORCE)
    else()
        if(NOT EXISTS "${BINARY_PKG_DIR}/build/native/include/WebView2.h")
            message(STATUS "Downloading Microsoft.Web.WebView2 NuGet package (${WEBVIEW2_VERSION})...")
            set(WEBVIEW2_ZIP "${CMAKE_BINARY_DIR}/webview2_${WEBVIEW2_VERSION}.zip")
            file(DOWNLOAD
                "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
                "${WEBVIEW2_ZIP}"
                SHOW_PROGRESS
            )
            file(ARCHIVE_EXTRACT
                INPUT "${WEBVIEW2_ZIP}"
                DESTINATION "${BINARY_PKG_DIR}"
            )
            file(REMOVE "${WEBVIEW2_ZIP}")
        endif()
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/packages" CACHE PATH "WebView2 location" FORCE)
    endif()
endif()

# ==============================================================================
# 2. Fetch and Configure JUCE 8 (8.0.6)
# ==============================================================================
if(NOT TARGET juce::juce_audio_processors)
    set(LOCAL_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../braun_as-42/build/_deps/juce-src")
    if(EXISTS "${LOCAL_JUCE_DIR}/CMakeLists.txt")
        message(STATUS "Using local JUCE 8 from ${LOCAL_JUCE_DIR}")
        FetchContent_Declare(
            JUCE
            SOURCE_DIR "${LOCAL_JUCE_DIR}"
        )
    else()
        message(STATUS "Fetching JUCE 8 (8.0.6)...")
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.6
            GIT_SHALLOW    TRUE
        )
    endif()
    FetchContent_MakeAvailable(JUCE)
endif()

# ==============================================================================
# 3. Fetch CLAP Extensions
# ==============================================================================
if(NOT TARGET clap_juce_extensions)
    set(LOCAL_CLAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../braun_rb-26/build/_deps/clap-juce-extensions-src")
    if(EXISTS "${LOCAL_CLAP_DIR}/CMakeLists.txt")
        message(STATUS "Using local clap-juce-extensions from ${LOCAL_CLAP_DIR}")
        FetchContent_Declare(
            clap-juce-extensions
            SOURCE_DIR "${LOCAL_CLAP_DIR}"
        )
    else()
        message(STATUS "Provisioning clap-juce-extensions...")
        FetchContent_Declare(
            clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG        main
            GIT_SUBMODULES_RECURSE TRUE
        )
    endif()
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

# ==============================================================================
# 4. ppf42_dsp_core Static Library (Pure C++20 Physical Modeling Core)
# ==============================================================================
add_library(ppf42_dsp_core STATIC
    source/dsp/DspDefines.h
    source/dsp/DspMath.h
    source/dsp/ParameterSnapshot.h
    source/dsp/MidiEvent.h
    source/dsp/SampleBank.h
    source/dsp/SampleBank.cpp
    source/dsp/SamplePitcherEngine.h
    source/dsp/SphincterOscillator.h
    source/dsp/SphincterOscillator.cpp
    source/dsp/FluidNoiseEngine.h
    source/dsp/FluidNoiseEngine.cpp
    source/dsp/CleftWaveguide.h
    source/dsp/CleftWaveguide.cpp
    source/dsp/PorcelainConvolver.h
    source/dsp/PorcelainConvolver.cpp
    source/dsp/SubBassDcBlocker.h
    source/dsp/BoundedSaturator.h
    source/dsp/VoiceManager.h
    source/dsp/VoiceManager.cpp
    source/dsp/TelemetryRingBuffer.h
    source/dsp/DspEngine.h
    source/dsp/DspEngine.cpp
)
target_include_directories(ppf42_dsp_core PUBLIC
    source
    source/dsp
)

# ==============================================================================
# 5. ppf42_headless_dsp_tests Executable (Automated Headless Test Suite)
# ==============================================================================
enable_testing()

add_executable(ppf42_headless_dsp_tests
    source/tests/TestHarness.h
    source/tests/AllocationTracker.h
    source/tests/AllocationTracker.cpp
    source/tests/UnitDspTests.cpp
    source/tests/BurnInDspTests.cpp
    source/tests/ConcurrencyDspTests.cpp
    source/tests/AdversarialDspTests.cpp
    source/tests/HeadlessMain.cpp
)
target_include_directories(ppf42_headless_dsp_tests PRIVATE
    source
    source/dsp
    source/tests
)
target_link_libraries(ppf42_headless_dsp_tests PRIVATE
    ppf42_dsp_core
)

add_test(NAME Ppf42HeadlessDspTests COMMAND ppf42_headless_dsp_tests)

# ==============================================================================
# 6. Main Audio Plugin Target: PPF-42_Dynamics (Configured when plugin source present)
# ==============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/PluginProcessor.cpp")
    # Package Web UI Assets if web directory exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html")
        set(WEB_ASSETS_ZIP "${CMAKE_CURRENT_BINARY_DIR}/web_assets_ppf42.zip")
        file(GLOB_RECURSE WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/web/*")
        add_custom_command(
            OUTPUT "${WEB_ASSETS_ZIP}"
            COMMAND ${CMAKE_COMMAND} -E tar cf "${WEB_ASSETS_ZIP}" --format=zip .
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web"
            DEPENDS ${WEB_FILES}
            COMMENT "Packaging PPF-42 Web UI assets..."
        )
        add_custom_target(PackagePpf42WebAssets DEPENDS "${WEB_ASSETS_ZIP}")
        juce_add_binary_data(Ppf42WebAssets SOURCES "${WEB_ASSETS_ZIP}")
        add_dependencies(Ppf42WebAssets PackagePpf42WebAssets)
    endif()

    juce_add_plugin(PPF42_Dynamics
        COMPANY_NAME "Poopenfarten Pro"
        BUNDLE_ID "com.poopenfarten.ppf42dynamics"
        VERSION "1.0.0"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Ppnf
        PLUGIN_CODE Ppf4
        FORMATS VST3 Standalone
        PRODUCT_NAME "PPF-42_Dynamics"
        NEEDS_WEBVIEW2 TRUE
    )

    file(GLOB_RECURSE PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/*")
    target_sources(PPF42_Dynamics PRIVATE ${PLUGIN_SOURCES})

    target_include_directories(PPF42_Dynamics PRIVATE
        source
        source/dsp
        source/plugin
        source/plugin/web
    )

    target_link_libraries(PPF42_Dynamics PRIVATE
        ppf42_dsp_core
        juce::juce_audio_utils
        juce::juce_gui_extra
    )

    if(TARGET Ppf42WebAssets)
        target_link_libraries(PPF42_Dynamics PRIVATE Ppf42WebAssets)
    endif()

    target_compile_definitions(PPF42_Dynamics PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    if(TARGET clap_juce_extensions)
        clap_juce_extensions_plugin(
            TARGET PPF42_Dynamics
            CLAP_ID "com.poopenfarten.ppf42dynamics"
            CLAP_FEATURES instrument synthesizer physical-modelling
        )
    endif()

    if(TARGET PPF42_Dynamics_Standalone)
        add_custom_command(TARGET PPF42_Dynamics_Standalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:PPF42_Dynamics_Standalone>
                "$<TARGET_FILE_DIR:PPF42_Dynamics_Standalone>/PPF-42_Dynamics_Standalone.exe"
            COMMENT "Creating PPF-42_Dynamics_Standalone.exe alias"
        )
    endif()
endif()
