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

# Ableton Link (licence GPLv2 ou ultérieure) : synchro du tempo et de la mesure avec
# Serato DJ, Ableton Live, Traktor... Téléchargé avec sa bibliothèque réseau asio.
FetchContent_Declare(link
    GIT_REPOSITORY https://github.com/Ableton/link.git
    GIT_TAG Link-3.1.5
    GIT_SHALLOW TRUE
    GIT_SUBMODULES "modules/asio-standalone"
    SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(link)
include(${link_SOURCE_DIR}/AbletonLinkConfig.cmake)

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
    Source/NoteEditor.cpp
    Source/ChannelEditor.cpp
    Source/Hub.cpp
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
    ICON_BIG Resources/icon.png
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Grid Play écoute les entrées de la carte son pour retravailler le son de vos synthés externes (retour audio)."
    DOCUMENT_EXTENSIONS gridplay)

target_sources(GridPlay PRIVATE ${GRIDPLAY_SOURCES})

# Police Source Sans 3 (Adobe, licence SIL OFL 1.1 : Resources/Fonts/OFL.txt), intégrée à l'appli.
juce_add_binary_data(GridPlayFonts SOURCES
    Resources/Fonts/SourceSans3-Regular.ttf
    Resources/Fonts/SourceSans3-Semibold.ttf
    Resources/Fonts/SourceSans3-Bold.ttf
    Resources/Fonts/SourceSans3-It.ttf)
target_link_libraries(GridPlay PRIVATE GridPlayFonts Ableton::Link)
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
