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

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release