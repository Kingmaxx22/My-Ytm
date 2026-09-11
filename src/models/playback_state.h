#pragma once

#include <cstdio>
#include <string>
#include <string_view>

namespace myytm::models {

enum class PlaybackStatus {
    Stopped,
    Playing,
    Paused,
};

inline std::string_view toString(PlaybackStatus s) noexcept
{
    switch (s) {
        case PlaybackStatus::Stopped: return "Stopped";
        case PlaybackStatus::Playing: return "Playing";
        case PlaybackStatus::Paused: return "Paused";
    }
    return "?";
}

struct PlaybackState {
    PlaybackStatus status = PlaybackStatus::Stopped;
    int volume = 50; // 0-100
    int positionSeconds = 0;
    int durationSeconds = 0; // 0 if unknown
    bool hasTrack = false;
    std::string currentTitle;
    std::string currentSubtitle;

    [[nodiscard]] std::string timeLabel() const
    {
        auto fmt = [](int s) {
            int m = s / 60;
            int sec = s % 60;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d:%02d", m, sec);
            return std::string(buf);
        };
        if (durationSeconds > 0) return fmt(positionSeconds) + " / " + fmt(durationSeconds);
        return fmt(positionSeconds);
    }
};

} // namespace myytm::models
