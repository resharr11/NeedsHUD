# Survival Thermometer and Needs HUD (NeedsHUD)

Source code of Survival Thermometer and Needs HUD, an SKSE mod for Skyrim Special Edition and Anniversary Edition that draws a thermometer and needs gauges into the vanilla HUD.

The mod itself (plugin, effects, compiled scripts) is on Nexus Mods:
https://www.nexusmods.com/skyrimspecialedition/mods/198628

This repository holds the code that builds `NeedsHUD.dll` (the SKSE plugin), the Papyrus script sources, and the text files the plugin reads.

## Game version

Built and tested on Skyrim 1.7.104. The plugin is built against the 1.7.99 and newer memory layouts. I don't make versions for older games, but the code is here for anyone who wants to port it.

## What is where

```
CMakeLists.txt, CMakePresets.json   build setup
vcpkg.json, vcpkg-configuration.json   libraries (spdlog, commonlib-shared)
build.bat                           builds the DLL with Visual Studio (see below)
check_paths.ps1                     checks the built DLL for folder paths and the Windows user name
cmake/                              version files, vcpkg overlay ports and triplet
extern/CommonLibSSE/                CommonLibSSE (powerof3), the copy this mod is built with
src/                                the SKSE plugin
  main.cpp                          plugin entry point
  ColdGauge.inl                     the thermometer
  *Tile.inl                         one gauge each (hunger, thirst, tiredness, disease...)
  ConfigLoader.inl                  reads the patch files other mods use to add their needs
  Menu.cpp, Settings.h              settings page (SKSE Menu Framework)
papyrus/Scripts/                    Papyrus script sources (bridges to SunHelm and iNeed, cold and mod bridges)
data/                               files that go into Skyrim's Data folder, as shipped
  SKSE/Plugins/NeedsHUD/NeedsHUD Patch Guide.txt   how other mods add their needs to the HUD
```

## Building the DLL

You need:

1. Visual Studio 2026 with the "Desktop development with C++" workload (it includes CMake and vcpkg).
2. Git.
3. A folder for the repository with no spaces anywhere in its path, for example `C:\Mods\NeedsHUD`. The build removes folder paths from the DLL, and that option stops with an error when the path two folders up has a space.

Then run `build.bat`. The first build downloads and builds the libraries, which takes a while; later builds are fast. The DLL ends up in `build\Release\NeedsHUD.dll`, and the last lines of the window say whether the build worked. If something fails, the details are in `build_log.txt`.

Without the batch file: open a "Developer Command Prompt for VS", set `VCPKG_ROOT` to your vcpkg folder, then run `cmake --preset release` and `cmake --build --preset release`.

## Compiling the scripts

Use the Creation Kit's Papyrus compiler on `papyrus/Scripts`. The bridge scripts call SunHelm and iNeed, so their script sources must be on the compiler's import list, along with Skyrim's own script sources.

## Licence

My code is licensed under the GNU General Public License, version 3 or later (see `LICENSE`). If you release something built from it, its source must be released under the same terms.

Third-party code in this repository keeps its own licence:

- CommonLibSSE by powerof3 and contributors: GPL-3.0-or-later with the Modding Exception (`extern/CommonLibSSE/COPYING.txt`, `extern/CommonLibSSE/EXCEPTIONS.md`).
- SKSE Menu Framework header (`src/SKSEMenuFramework.h`): LGPL-2.1 (`src/SKSEMenuFramework.LICENSE.txt`).
- commonlib-shared and spdlog are downloaded by vcpkg during the build and keep their own licences.

The plugin (`NeedsHUD.esp`) and the effect animations (`Interface/NeedsHUD/*.swf`) are not in this repository; their permissions are on the Nexus page.

## Bug reports

Please report bugs on the Nexus page's Bugs tab, with your `NeedsHUD.log` (Documents\My Games\Skyrim Special Edition\SKSE).
