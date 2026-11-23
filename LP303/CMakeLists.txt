cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), à partir de macOS 10.13.
# Ces réglages sont ignorés sous Windows et Linux.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Version minimale de macOS")

project(LP303 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LP303_INSTALL_PLUGINS "Copier les plugins dans les dossiers système après compilation" ON)

# ---------------------------------------------------------------------------
# Dépendances : prises dans le dossier du projet si elles y sont (en local,
# liens vers celles de MiniMoog/), sinon téléchargées aux mêmes versions que
# pour LP-60 et Model Play.
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/clap-juce-extensions/CMakeLists.txt")
    add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG 6bde108329ff9fc24404b79b8b397261a75cf6a8
        GIT_SUBMODULES_RECURSE TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

# ---------------------------------------------------------------------------
# LP-303, basse acide monophonique avec séquenceur 16 pas.
# Le plugin : AU (Logic, GarageBand, MainStage...), VST3 (Ableton, Cubase,
# Studio One, FL Studio...), CLAP (Bitwig, Reaper...), LV2 (Ardour, Carla,
# Linux / Raspberry Pi) et une appli autonome pour jouer sans DAW.
# AU n'existe que sur Mac : il est ignoré ailleurs.
# ---------------------------------------------------------------------------
juce_add_plugin(LP303
    COMPANY_NAME "Load & Play"
    BUNDLE_ID com.loadandplay.lp303
    PRODUCT_NAME "LP-303"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${LP303_INSTALL_PLUGINS}
    PLUGIN_MANUFACTURER_CODE LdPl
    PLUGIN_CODE L303
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    LV2URI "urn:loadandplay:lp303"
    FORMATS AU VST3 LV2 Standalone)

clap_juce_extensions_plugin(TARGET LP303
    CLAP_ID "com.loadandplay.lp303"
    CLAP_FEATURES instrument synthesizer "virtual analog" mono)

target_sources(LP303 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(LP303 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Les sources contiennent des accents : on le précise au compilateur Microsoft.
if(MSVC)
    target_compile_options(LP303 PUBLIC /utf-8)
endif()

target_link_libraries(LP303
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests hors DAW (filtre, slide, accent, séquenceur) et rendu d'une démo WAV.
# ---------------------------------------------------------------------------
juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")

target_sources(RenderTest PRIVATE Tests/RenderTest.cpp)

target_compile_definitions(RenderTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

if(MSVC)
    target_compile_options(RenderTest PRIVATE /utf-8)
endif()

target_link_libraries(RenderTest
    PRIVATE
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Outil de développement : capture PNG de l'interface (-DLP303_SNAPSHOT=ON).
# ---------------------------------------------------------------------------
option(LP303_SNAPSHOT "Construire l'outil de capture de l'interface" OFF)

if(LP303_SNAPSHOT)
    juce_add_gui_app(Snapshot PRODUCT_NAME "Snapshot")

    target_sources(Snapshot PRIVATE
        Tests/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(Snapshot PRIVATE
        JucePlugin_Name="LP-303"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(Snapshot
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags)
endif()
