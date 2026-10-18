cmake_minimum_required(VERSION 3.22)

project(MiPropioDaw VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# App principal
# ----------------------------------------------------------------------------
juce_add_gui_app(MiPropioDaw
    PRODUCT_NAME   "MiPropioDaw"
    COMPANY_NAME   "MiDaw"
    BUNDLE_ID      com.midaw.miproiodaw
    NEEDS_CURL     FALSE
    NEEDS_WEB_BROWSER FALSE)

juce_generate_juce_header(MiPropioDaw)

target_sources(MiPropioDaw PRIVATE
    src/Main.cpp
    src/core/ProjectState.cpp
    src/audio/AudioEngine.cpp
    src/model/TrackList.cpp
    src/ui/LookAndFeel_Dark.cpp
    src/ui/SplitPane.cpp
    src/ui/MainLayout.cpp
    src/ui/MainWindow.cpp
    src/ui/widgets/TransportWidgets.cpp
    src/ui/widgets/TrackHeader.cpp
    src/ui/widgets/MixerStrip.cpp
    src/ui/widgets/VUMeter.cpp
    src/ui/widgets/SpectrumDisplay.cpp
    src/ui/widgets/ProChannelPanel.cpp
    src/ui/panels/TransportBar.cpp
    src/ui/panels/BrowserPanel.cpp
    src/ui/panels/PatternSidebar.cpp
    src/ui/panels/ArrangementView.cpp
    src/ui/panels/MixerView.cpp
    src/ui/panels/MidiEditorPanel.cpp)

target_include_directories(MiPropioDaw PRIVATE src)

target_compile_definitions(MiPropioDaw PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(MiPropioDaw
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------------------
# Tests de modelo (sin GUI)
# ----------------------------------------------------------------------------
juce_add_console_app(MiPropioDawTests PRODUCT_NAME "MiPropioDawTests")

juce_generate_juce_header(MiPropioDawTests)

target_sources(MiPropioDawTests PRIVATE
    tests/TestsMain.cpp
    src/core/ProjectState.cpp
    src/audio/AudioEngine.cpp
    src/model/TrackList.cpp)

target_include_directories(MiPropioDawTests PRIVATE src)

target_compile_definitions(MiPropioDawTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(MiPropioDawTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
