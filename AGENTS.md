# AGENTS.md — My-Ytm

## Project Overview

My-Ytm is a Windows-first YouTube Music CLI/TUI written in modern C++.

The goal is to provide a lightweight terminal music client with:

- YouTube Music account authentication
- Search
- Playback
- Queue management
- Library access
- Playlists
- History
- Vim-style navigation
- Keyboard-driven controls
- A simple, fast terminal interface

The application should prioritize keyboard interaction and remain usable without a mouse.

---

# Technology

- Language: C++
- Standard: C++20
- Build system: CMake
- Primary platform: Windows
- Interface: CLI/TUI
- Compiler target: MSVC
- Authentication: Browser-based authentication
- Version control: Git

Use portable C++ wherever practical.

Windows-specific functionality should be isolated behind small interfaces when possible.

---

# Repository Structure

```text
My-Ytm/
├── AGENTS.md
├── CMakeLists.txt
├── README.md
│
├── cmake/
├── docs/
│
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── auth/
│   ├── config/
│   ├── models/
│   ├── player/
│   ├── ui/
│   └── youtube/
│
├── tests/
└── third_party/
```

Do not create files or directories without a reason.

---

# Architecture

The project should follow a layered architecture:

```text
┌─────────────────────┐
│         UI          │
├─────────────────────┤
│     Application     │
├─────────────────────┤
│ Services / Models   │
├─────────────────────┤
│ YouTube / Player    │
├─────────────────────┤
│ Platform / Network  │
└─────────────────────┘
```

Dependencies should generally point downward.

The UI should not directly implement HTTP requests, authentication protocols, YouTube response parsing, playback backend logic, or persistent credential handling.

---

# `src/main.cpp`

`main.cpp` should remain minimal.

It is responsible for:

1. Starting the application
2. Creating the top-level application object
3. Running the application
4. Returning the exit code

Do not place major business logic in `main.cpp`.

Preferred pattern:

```cpp
int main()
{
    App app;
    return app.run();
}
```

---

# `src/app/`

Application lifecycle and high-level coordination.

Responsibilities:

- Application startup
- Dependency initialization
- Main event loop
- Application state
- Shutdown
- Connecting UI, player, authentication, and services

The App layer should coordinate components rather than contain their implementation.

---

# `src/auth/`

Authentication and session management.

Authentication must be browser-based.

## Never

Never ask the user to enter their Google password directly into My-Ytm.

Never commit or print:

- Passwords
- Cookies
- Access tokens
- Refresh tokens
- Authentication headers
- Session secrets
- Browser credentials

Sensitive authentication information must never appear in:

- Git
- Logs
- Debug output
- Error messages
- Screenshots
- Test fixtures

Use secure Windows storage where appropriate.

Authentication implementation must comply with applicable YouTube/Google terms and supported authentication mechanisms.

Do not implement mechanisms intended to bypass authentication or access controls.

---

# `src/config/`

Persistent application configuration.

Potential settings include:

- Keybindings
- Volume
- UI preferences
- Player preferences
- Cache locations
- Application settings

Use `std::filesystem` for filesystem operations.

Configuration should be separated from authentication secrets.

Do not store sensitive credentials in ordinary configuration files.

---

# `src/models/`

Shared data structures.

Examples:

- `Song`
- `Artist`
- `Album`
- `Playlist`
- `SearchResult`
- `QueueItem`
- `PlaybackState`
- `UserAccount`

Models should primarily contain data, validation, and lightweight behavior.

Avoid putting network requests or UI logic into models.

---

# `src/player/`

Playback abstraction and queue management.

The UI must not depend directly on a specific playback implementation.

Use an abstraction such as:

```cpp
class Player
{
public:
    virtual ~Player() = default;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void next() = 0;
    virtual void previous() = 0;
};
```

Responsibilities include:

- Play
- Pause
- Stop
- Next
- Previous
- Seeking
- Volume
- Playback state
- Queue management

---

# `src/ui/`

Terminal user interface.

Responsibilities:

- Rendering
- Input handling
- Screens/views
- Navigation
- Search input
- Keybindings
- Status bar
- Help screen
- User feedback

UI code must not contain YouTube protocol implementation.

Keep the interface simple and information-dense.

Avoid:

- Mouse dependency
- Excessive panels
- Unnecessary animations
- Deep nested menus
- Large graphical layouts
- Excessive terminal redraws

---

# Vim-Style Navigation

The application should feel familiar to Vim users.

## Navigation

```text
j           Move down
k           Move up
h           Go back
l           Open
Enter       Activate
gg          First item
G           Last item
Ctrl-d      Half page down
Ctrl-u      Half page up
```

## Search

```text
/           Search
n           Next result
N           Previous result
Esc         Exit search/input mode
```

## Playback

```text
Space       Play / pause
]           Next
[           Previous
H           Seek backward
L           Seek forward
+           Volume up
-           Volume down
```

## Queue

```text
a           Add to queue
A           Add to end of queue
d           Remove
c           Clear queue
```

## Application

```text
1           Home
2           Search
3           Library
4           Playlists
5           History
?           Help
q           Quit / back
```

Avoid unnecessarily overloading keys.

Context-sensitive keybindings should be displayed in the help screen.

---

# YouTube Integration

`src/youtube/` contains service-specific functionality.

Potential responsibilities:

- Search
- Song metadata
- Artists
- Albums
- Playlists
- Library
- History
- Account information
- Response parsing

Keep YouTube-specific implementation isolated.

Preferred architecture:

```text
UI
 │
 ▼
Application
 │
 ▼
YouTubeClient
 │
 ▼
YouTube service
```

Avoid putting HTTP, authentication, JSON parsing, playback, and service-specific logic directly into UI code.

---

# Networking

Network access should be isolated behind service/client classes.

Do not perform HTTP requests directly from UI code.

External responses are untrusted input.

Always:

- Validate responses
- Handle missing fields
- Handle malformed data
- Handle network failures
- Handle authentication failures
- Handle rate limits
- Handle timeouts

Never assume a remote response is valid.

---

# Error Handling

Errors should be handled explicitly.

Do not silently ignore failures.

User-facing errors should be understandable:

```text
Unable to connect to YouTube Music.
Check your internet connection and try again.
```

Avoid exposing tokens, cookies, HTTP authorization headers, internal secrets, or sensitive request data.

Debug logging may contain technical information, but never authentication secrets.

---

# C++ Guidelines

Use modern C++20.

Prefer:

- RAII
- `std::string`
- `std::string_view`
- `std::vector`
- `std::optional`
- `std::variant`
- `std::unique_ptr`
- `std::shared_ptr` when shared ownership is actually required
- `std::filesystem`
- `enum class`
- `constexpr`
- Strong types

Avoid unnecessary:

- Raw owning pointers
- Manual memory management
- Global mutable state
- Macros
- Singleton-heavy designs
- Deep inheritance hierarchies

Ownership should be obvious from the code.

Prefer composition over inheritance unless an interface is genuinely useful.

---

# Platform-Specific Code

The primary platform is Windows.

Windows-specific functionality may be required for:

- Credential storage
- Terminal handling
- Process management
- Browser launching
- System integration
- Audio playback

Keep platform-specific code isolated.

Prefer:

```text
src/platform/
```

for platform implementations if the project grows enough to require them.

Do not spread Windows API calls throughout unrelated modules.

---

# Dependencies

Prefer small, well-maintained dependencies.

Before adding a dependency:

1. Determine whether the standard library is sufficient.
2. Determine whether the dependency is actively maintained.
3. Check its license.
4. Consider Windows compatibility.
5. Consider build complexity.
6. Consider whether it is actually necessary.

Third-party dependencies belong in:

```text
third_party/
```

when vendored.

Do not blindly vendor large libraries.

---

# Tests

Tests belong in:

```text
tests/
```

New non-trivial behavior should have tests where practical.

Prioritize testing:

- Models
- Queue behavior
- Configuration
- Key handling
- Search parsing
- Authentication state handling
- Player state
- Error handling

Tests must never contain real credentials or real authentication tokens.

---

# Build

The project uses CMake.

Standard Windows build:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Run the application:

```powershell
.\build\Release\MyYtm.exe
```

Build output belongs in:

```text
build/
```

Build artifacts must not be committed to Git.

---

# Git

Use focused commits.

Preferred commit style:

```text
feat: add terminal application skeleton
feat: add vim keymap
feat: add search models
feat: add youtube client
feat: add authentication
feat: add playback queue
fix: handle empty search results
refactor: isolate player backend
test: add queue tests
docs: update authentication notes
chore: update build configuration
```

Do not create giant commits containing unrelated changes.

Do not force-push or rewrite history unless explicitly requested.

Do not commit:

- `build/`
- `.vs/`
- IDE metadata
- Credentials
- Tokens
- Cookies
- Local configuration containing secrets
- Generated binaries

---

# Development Phases

## Phase 1 — Foundation

- CMake
- Executable
- Application class
- Terminal initialization
- Main event loop

## Phase 2 — UI

- Screen abstraction
- Rendering
- Input handling
- Vim keymap
- Status bar
- Help screen

## Phase 3 — Search

- Song model
- Artist model
- Album model
- Search results
- Search screen
- `/` search interaction

## Phase 4 — YouTube Client

- HTTP abstraction
- YouTube client
- Response parsing
- Search integration

## Phase 5 — Authentication

- Browser authentication flow
- Session management
- Secure credential storage
- Account state

## Phase 6 — Playback

- Player interface
- Playback backend
- Queue
- Play/pause
- Next/previous
- Seeking
- Volume

## Phase 7 — Library

- Liked music
- Playlists
- History
- Albums
- Artists

## Phase 8 — Polish

- Configuration
- Error handling
- Logging
- Windows packaging
- Installer/release builds
- Performance improvements

---

# Development Philosophy

Prioritize:

1. Correctness
2. Simplicity
3. Maintainability
4. Keyboard usability
5. Windows compatibility
6. Performance
7. Small, focused components

Do not prematurely optimize.

Do not add features merely because they are technically interesting.

Every subsystem should have a clear responsibility.

The terminal UI should remain fast and uncluttered.

---

# Security Rules

Security-sensitive code requires extra care.

Never:

- Request Google passwords
- Commit credentials
- Log authentication tokens
- Print cookies
- Store secrets in Git
- Hard-code API secrets
- Disable TLS verification
- Bypass authentication controls
- Circumvent access restrictions

Treat all network data as untrusted.

If a feature requires handling sensitive authentication data, isolate it and document the security implications.

---

# Agent Instructions

When modifying this repository:

1. Read `AGENTS.md` before making architectural changes.
2. Inspect existing code before creating new abstractions.
3. Keep changes focused.
4. Avoid unrelated refactors.
5. Preserve existing behavior unless the task requires changing it.
6. Build after significant C++ changes.
7. Run relevant tests after changing tested behavior.
8. Do not commit generated build files.
9. Do not introduce secrets.
10. Update documentation when architecture or behavior changes.
11. Prefer small, reviewable commits.
12. Do not rewrite Git history unless explicitly requested.

When unsure about architecture, prefer the simplest design that preserves future flexibility.

---

# Current Project Status

The repository is currently in the initial foundation stage.

Current executable:

```text
My-Ytm
```

Current entry point:

```text
src/main.cpp
```

The initial program should successfully build and print:

```text
My-Ytm
```

Future implementation should build incrementally from this foundation.
