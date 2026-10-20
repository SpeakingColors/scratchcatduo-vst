cmake_minimum_required(VERSION 3.22)
project(scratchcatduo VERSION 0.2.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)
FetchContent_Declare(cat_ui GIT_REPOSITORY https://github.com/SpeakingColors/cat-ui GIT_TAG main)
FetchContent_MakeAvailable(cat_ui)

juce_add_plugin(ScratchCatDuo
    COMPANY_NAME "Studio 87"
    PLUGIN_MANUFACTURER_CODE Spco
    PLUGIN_CODE Scdu
    FORMATS VST3 Standalone
    PRODUCT_NAME "Scratch Cat Duo"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT TRUE)
target_sources(ScratchCatDuo PRIVATE src/Plugins.cpp src/duo/Duo.cpp src/duo/DuoEditor.cpp)
target_include_directories(ScratchCatDuo PRIVATE src)
target_compile_definitions(ScratchCatDuo PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(ScratchCatDuo PRIVATE cat_ui juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
