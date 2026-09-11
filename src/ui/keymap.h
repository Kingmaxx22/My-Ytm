#pragma once

#include "ui/key.h"

#include <string_view>

namespace myytm::ui {

// Central place documenting vim-style bindings per AGENTS.md.
// No state — helpers for screens to interpret Keys consistently.

namespace keymap {

// Navigation
constexpr bool isDown(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'j' || k.code == KeyCode::ArrowDown; }
constexpr bool isUp(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'k' || k.code == KeyCode::ArrowUp; }
constexpr bool isBack(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'h' || k.code == KeyCode::ArrowLeft; }
constexpr bool isOpen(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'l' || k.code == KeyCode::ArrowRight || k.code == KeyCode::Enter; }
constexpr bool isFirst(const Key& k, bool pendingG) noexcept { return pendingG && k.code == KeyCode::Char && k.ch == 'g'; }
constexpr bool isLast(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'G'; }
constexpr bool isHalfPageDown(const Key& k) noexcept { return k.ctrl && k.code == KeyCode::Char && k.ch == 'd'; }
constexpr bool isHalfPageUp(const Key& k) noexcept { return k.ctrl && k.code == KeyCode::Char && k.ch == 'u'; }

// Search
constexpr bool isSearch(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '/'; }
constexpr bool isNextResult(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'n'; }
constexpr bool isPrevResult(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'N'; }

// Playback
constexpr bool isPlayPause(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == ' '; }
constexpr bool isNext(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == ']'; }
constexpr bool isPrev(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '['; }
constexpr bool isSeekBack(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'H'; }
constexpr bool isSeekForward(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'L'; }
constexpr bool isVolUp(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '+'; }
constexpr bool isVolDown(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '-'; }

// Queue
constexpr bool isAddToQueue(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'a'; }
constexpr bool isAddToEnd(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'A'; }
constexpr bool isRemove(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'd'; }
constexpr bool isClearQueue(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'c'; }

// App
constexpr bool isHome(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '1'; }
constexpr bool isSearchScreen(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '2'; }
constexpr bool isLibrary(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '3'; }
constexpr bool isPlaylists(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '4'; }
constexpr bool isHistory(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '5'; }
constexpr bool isHelp(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == '?'; }
constexpr bool isQuit(const Key& k) noexcept { return k.code == KeyCode::Char && k.ch == 'q' || k.code == KeyCode::Escape; }

} // namespace keymap

} // namespace myytm::ui
