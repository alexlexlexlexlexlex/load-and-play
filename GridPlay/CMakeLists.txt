cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), à partir de macOS 10.13.
# Ces réglages sont ignorés sous Windows et Linux.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Version minimale de macOS")

project(GridPlay VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE : pris dans le dossier du projet s'il y est, sinon téléchargé à la même
# version que pour les plugins.
# ---------------------------------------------------------------------------
include(FetchContent)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Grid Play : lanceur de boucles MIDI façon session d'Ableton Live, qui joue des
# instruments virtuels (VST3, AU, LV2) ou des ports MIDI, piloté à la souris, au
# clavier d'ordinateur, aux claviers MIDI et à l'Akai APC mini (mk1 et mk2).
# ---------------------------------------------------------------------------
set(GRIDPLAY_SOURCES
    Source/Main.cpp
    Source/Session.cpp
    Source/AudioEngine.cpp
    Source/ApcMini.cpp
    Source/SessionView.cpp
    Source/MainComponent.cpp)

set(GRIDPLAY_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GridPlay,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GridPlay,JUCE_VERSION>")

juce_add_gui_app(GridPlay
    COMPANY_NAME "Load & Play"
    BUNDLE_ID com.loadandplay.gridplay
    PRODUCT_NAME "Grid Play"
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED FALSE
    DOCUMENT_EXTENSIONS gridplay)

target_sources(GridPlay PRIVATE ${GRIDPLAY_SOURCES})
target_compile_definitions(GridPlay PRIVATE ${GRIDPLAY_DEFINITIONS})

# Les Audio Units n'existent que sur Mac.
if(APPLE)
    target_compile_definitions(GridPlay PRIVATE JUCE_PLUGINHOST_AU=1)
    target_link_libraries(GridPlay PRIVATE "-framework CoreAudioKit")
endif()

# Les sources contiennent des accents : on le précise au compilateur Microsoft.
if(MSVC)
    target_compile_options(GridPlay PRIVATE /utf-8)
endif()

target_link_libraries(GridPlay
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests du séquenceur et du protocole de l'APC mini, sans carte son ni MIDI.
# ---------------------------------------------------------------------------
juce_add_console_app(SessionTest PRODUCT_NAME "SessionTest")

target_sources(SessionTest PRIVATE
    Tests/SessionTest.cpp
    Source/Session.cpp)

target_compile_definitions(SessionTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

if(MSVC)
    target_compile_options(SessionTest PRIVATE /utf-8)
endif()

target_link_libraries(SessionTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
