#pragma once

#include "models/playback_state.h"

namespace myytm::player {

class Player {
public:
    virtual ~Player() = default;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void next() = 0;
    virtual void previous() = 0;

    virtual void seek(int deltaSeconds) = 0; // H/L
    virtual void setVolume(int volume) = 0;  // 0-100, +/- steps
    virtual void volumeUp(int step = 5) = 0;
    virtual void volumeDown(int step = 5) = 0;

    [[nodiscard]] virtual models::PlaybackState state() const = 0;
    [[nodiscard]] virtual bool isPlaying() const noexcept = 0;
};

} // namespace myytm::player
