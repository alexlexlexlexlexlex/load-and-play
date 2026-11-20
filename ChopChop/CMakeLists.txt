cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), à partir de macOS 10.13.
# Ces réglages sont ignorés sous Windows et Linux.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Version minimale de macOS")

project(ChopChop VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CHOPCHOP_INSTALL_PLUGINS "Copier les plugins dans les dossiers système après compilation" ON)

# ---------------------------------------------------------------------------
# Dépendances : prises dans le dossier du projet si elles y sont, sinon
# téléchargées à la même version que pour les autres plugins Load & Play.
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
# Chop Chop, repeater de voix distordu : séquenceur, drums en sidechain ou MIDI.
# VST3 (Cubase, Nuendo, Ableton, Studio One, FL Studio, Reaper...), AU (Logic,
# GarageBand), CLAP (Bitwig, Reaper) et LV2 (Ardour, Carla, Linux).
# Effet avec entrée MIDI : en AU, un « music effect » (aumf).
# ---------------------------------------------------------------------------
juce_add_plugin(ChopChop
    COMPANY_NAME "Load & Play"
    BUNDLE_ID com.loadandplay.chopchop
    PRODUCT_NAME "Chop Chop"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${CHOPCHOP_INSTALL_PLUGINS}
    PLUGIN_MANUFACTURER_CODE LdPl
    PLUGIN_CODE ChCh
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    VST3_CATEGORIES Fx Distortion
    LV2URI "urn:loadandplay:chopchop"
    FORMATS AU VST3 LV2)

clap_juce_extensions_plugin(TARGET ChopChop
    CLAP_ID "com.loadandplay.chopchop"
    CLAP_FEATURES audio-effect distortion glitch)

target_sources(ChopChop PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ChopChop PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Les sources contiennent des accents : on le précise au compilateur Microsoft.
if(MSVC)
    target_compile_options(ChopChop PUBLIC /utf-8)
endif()

target_link_libraries(ChopChop
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests du moteur hors DAW.
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
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Outils de développement : capture PNG de l'interface (-DCHOPCHOP_SNAPSHOT=ON)
# et hôte qui charge les plugins compilés comme un DAW (-DCHOPCHOP_HOSTTEST=ON).
# ---------------------------------------------------------------------------
option(CHOPCHOP_SNAPSHOT "Construire l'outil de capture de l'interface" OFF)

if(CHOPCHOP_SNAPSHOT)
    juce_add_gui_app(Snapshot PRODUCT_NAME "Snapshot")

    target_sources(Snapshot PRIVATE
        Tests/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(Snapshot PRIVATE
        JucePlugin_Name="Chop Chop"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(Snapshot
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags)
endif()

option(CHOPCHOP_HOSTTEST "Construire l'hôte de test des plugins compilés" OFF)

if(CHOPCHOP_HOSTTEST)
    juce_add_gui_app(HostTest PRODUCT_NAME "HostTest")

    target_sources(HostTest PRIVATE Tests/HostTest.cpp)

    target_compile_definitions(HostTest PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(HostTest
        PRIVATE
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags)

    if(APPLE)
        target_link_libraries(HostTest PRIVATE "-framework CoreAudioKit")
    endif()
endif()
