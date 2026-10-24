cmake_minimum_required(VERSION 3.22)
project(Angelead VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

# JUCE folder must sit next to this file (git clone https://github.com/juce-framework/JUCE)
add_subdirectory(JUCE)

juce_add_plugin(Angelead
    COMPANY_NAME "Angelead"
    PLUGIN_MANUFACTURER_CODE Angl
    PLUGIN_CODE Angd
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 Standalone
    PRODUCT_NAME "Angelead"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Assets/angelead_icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/angelead_icon.png")

juce_add_binary_data(AngeleadAssets SOURCES Assets/angelead_girl.png)

target_sources(Angelead PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Angelead PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Angelead
    PRIVATE
        AngeleadAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
