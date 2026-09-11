# My-Ytm

A Windows-first YouTube Music CLI/TUI written in C++.

## Goals

- YouTube Music account authentication
- Terminal-based UI
- Vim-style navigation
- Search
- Playback
- Queue management
- Library
- Playlists
- History

## Requirements

- Windows
- C++20 compiler
- CMake 3.25+

## Features (Phases 1-8)

- Vim keys: `j/k/h/l/gg/G/Ctrl-d/u`, `/ n N Esc`, `Space ] [ H L +/-`, `a A d c`, `1-5 0 6 ? q`
- YouTube client with `MockHttpClient` + strict JSON parse, isolated HTTP per AGENTS.md
- Browser-based auth via DPAPI (`%APPDATA%\MyYtm\*.bin`), never logs tokens
- Player/Queue (`MockPlayer`) — play/pause/next/prev/seek/volume
- Library/Playlists/History screens backed by `YouTubeClient`
- Config: `%APPDATA%\MyYtm\config.json` (volume/theme), Logger: `myytm.log`

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\MyYtm.exe          # interactive TUI (pipes fallback to "My-Ytm")
ctest --test-dir build -C Release  # UI tests (75 tests)
```

## Installer (MSI — choosable install dir)

```powershell
# MSI via WiX v4 (WixUI_InstallDir — Browse button in UI)
dotnet tool install --global wix --version 4.0.5
wix extension add --global WixToolset.UI.wixext/4.0.5
cmake --build build --target msi --config Release
# -> build/MyYtm-0.1.0.msi  (per-machine, Browse for INSTALLFOLDER)

# Interactive install (UI with directory chooser):
msiexec /i build\MyYtm-0.1.0.msi

# Silent install to custom folder:
msiexec /i build\MyYtm-0.1.0.msi INSTALLFOLDER="D:\MyApps\MyYtm" /qn

# Also: ZIP via CPack
cpack --config build/CPackConfig.cmake -G ZIP
```