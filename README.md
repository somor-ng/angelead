# Angelead – VST3 synth for FL Studio (JUCE)

## Build (Windows, ~10 min)
1. Install Visual Studio 2022 (Desktop C++), CMake 3.22+ and Git.
2. In this folder:  git clone --depth 1 https://github.com/juce-framework/JUCE.git
3. cmake -B build -G "Visual Studio 17 2022"
4. cmake --build build --config Release
5. The VST3 is auto-copied to C:\Program Files\Common Files\VST3\Angelead.vst3
6. FL Studio: Options > Manage plugins > Find installed plugins (verify scan).
   Add it from the plugin database (Generators > Angelead).

## Plugin icon in FL Studio
FL Studio draws its own icon for VST3 plugins in the browser. Right-click the plugin entry in the
Plugin Database (or the wrapper's settings) > "Set icon..." and pick Assets/angelead_icon.ico.
The girl image is also embedded in the plugin's header and used as the standalone-app icon.
