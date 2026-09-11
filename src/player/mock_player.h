#pragma once

#include "models/playback_state.h"
#include "player/player.h"
#include "player/queue.h"

#include <memory>

namespace myytm::player {

class MockPlayer final : public Player {
public:
    explicit MockPlayer(std::shared_ptr<Queue> queue) : queue_(std::move(queue)) {}

    void play() override;
    void pause() override;
    void stop() override;
    void next() override;
    void previous() override;

    void seek(int deltaSeconds) override;
    void setVolume(int volume) override;
    void volumeUp(int step = 5) override;
    void volumeDown(int step = 5) override;

    [[nodiscard]] models::PlaybackState state() const override;
    [[nodiscard]] bool isPlaying() const noexcept override { return state_.status == models::PlaybackStatus::Playing; }

    [[nodiscard]] std::shared_ptr<Queue> queue() const noexcept { return queue_; }

private:
    void syncTrackFromQueue();

    std::shared_ptr<Queue> queue_;
    models::PlaybackState state_;
};

} // namespace myytm::player
