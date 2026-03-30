# Source Scene Finder for OBS

Ever lose track of which scenes a source is used in? This plugin adds right-click context menu options to any source in OBS, letting you instantly find and jump to every scene that uses it.

## Features

- **Find scenes using a source** - Right-click any source to see a list of all other scenes that contain it, then click to jump directly to that scene.
- **Jump into nested scenes** - Right-click a scene that's nested inside another scene and jump straight to the original scene to edit it.

## Installation

1. Download the latest `obs-source-scene-finder.dll` from the [Releases](https://github.com/WaffleSmacker/obs-source-scene-finder/releases) page.
2. Copy the DLL to your OBS plugins folder:
   - **Standard install:** `C:\Program Files\obs-studio\obs-plugins\64bit\`
   - **Custom install:** `<your OBS path>\obs-plugins\64bit\`
3. Restart OBS.

## Usage

1. Right-click any source in the **Sources** panel.
2. Near the top of the context menu you'll see:
   - **"Jump to Original Scene: [name]"** - Shown when the source is a nested scene. Click to switch to that scene.
   - **"Source used in N other scenes"** - A submenu listing every other scene that contains this source. Click any scene name to jump to it.

## Building from Source

### Requirements

- Visual Studio 2022
- CMake 3.28+

### Build Steps

```bash
cmake --preset windows-x64
cmake --build --preset windows-x64
```

Dependencies (OBS SDK, Qt6) are downloaded automatically during the configure step.

## License

This project is licensed under the GNU General Public License v2.0 - see the [LICENSE](LICENSE) file for details.
