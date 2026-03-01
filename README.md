# WinTools

WinTools is a modular Windows desktop utility app. Each module is a self-contained tool that enhances a different part of your Windows experience — all accessible from a single system-tray icon.

## Modules

| Module | Description |
| --- | --- |
| [MediaBar](https://github.com/itsCattie/WinTools/tree/main/src/modules/MediaBar) | A compact, always-on-top media control bar for controlling playback without switching windows. |
| [AudioMaster](https://github.com/itsCattie/WinTools/tree/main/src/modules/AudioMaster) | Manage audio devices and quickly switch default input/output without opening Windows settings. |
| [DiskSentinel](https://github.com/itsCattie/WinTools/tree/main/src/modules/DiskSentinel) | Monitor disk usage, health, and storage layout with a treemap visualiser. |
| [AdvancedTaskManager](https://github.com/itsCattie/WinTools/tree/main/src/modules/AdvancedTaskManager) | A detailed process and performance monitor with real-time CPU, memory, and network graphs. |
| [GameVault](https://github.com/itsCattie/WinTools/tree/main/src/modules/GameVault) | A unified game library that aggregates Steam, Epic, GOG, and emulator titles into one browsable grid. |
| [StreamVault](https://github.com/itsCattie/WinTools/tree/main/src/modules/StreamVault) | A streaming hub that searches movies and TV shows and shows exactly which services carry them in your country, with one-click redirects to Netflix, Disney+, Max, Hulu, and more. |

## Requirements

- A free [TMDB API key](https://www.themoviedb.org/settings/api) for StreamVault search
- Use the [Spotify for Developer Page](https://developer.spotify.com/dashboard) for spotify for MediaBar.

## Building

.\scripts\build-and-run.ps1

or

cmake -S . -B build-cpp -G Ninja \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.9.3/mingw_64/lib/cmake" \
  -DCMAKE_BUILD_TYPE=Debug

ninja -C build-cpp
