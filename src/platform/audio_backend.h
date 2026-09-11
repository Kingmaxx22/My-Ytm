#pragma once
#include "player/player.h"
#include "player/queue.h"
#include "models/playback_state.h"
#include "miniaudio.h"
#include <memory>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace myytm::platform {

// Real audio backend — isolated in platform layer per AGENTS.md.
// PCM output via miniaudio (WASAPI on Windows). Decoded streams arrive through
// playPcm(); startTone() synthesizes a sine into the same PCM path and exists
// only as a self-test/legacy placeholder for the pre-decoder player path.
// Raw sample signature (no player-layer types) keeps the Platform layer
// dependency-free per the layered architecture.
class WinAudioBackend {
public:
    WinAudioBackend() = default;
    ~WinAudioBackend();

    // Interleaved s16 PCM. Reinitializes the device when rate/channels change.
    // loop=true replays the buffer until stop() (used by startTone).
    bool playPcm(const int16_t* samples, size_t sampleCount, uint32_t sampleRateHz,
                 uint16_t channels, bool loop = false);
    bool startTone(int freqHz = 440, int durationMs = -1); // -1 = looped until stop
    void stop();
    void setVolume(int vol01); // 0-100
    [[nodiscard]] bool isPlaying() const noexcept { return playing_; }

private:
    static void deviceCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);
    void stopDeviceLocked();
    std::atomic<bool> playing_{false};
    std::mutex mutex_;
    int volume_ = 50;
    ma_device device_{};
    bool deviceInit_ = false;
    std::deque<int16_t> fifo_;
    std::vector<int16_t> loopBuf_;
    bool loop_ = false;
};

// Player implementation backed by WinAudioBackend + Queue.
// Keeps all MockPlayer queue logic but routes actual audio through platform.
class PlatformAudioPlayer final : public player::Player {
public:
    explicit PlatformAudioPlayer(std::shared_ptr<player::Queue> q);
    ~PlatformAudioPlayer() override;

    void play() override;
    void pause() override;
    void stop() override;
    void next() override;
    void previous() override;
    void seek(int deltaSeconds) override;
    void setVolume(int volume) override;
    void volumeUp(int step) override;
    void volumeDown(int step) override;
    [[nodiscard]] models::PlaybackState state() const override;
    [[nodiscard]] bool isPlaying() const noexcept override;

    [[nodiscard]] std::shared_ptr<player::Queue> queue() const noexcept { return queue_; }
    // PCM sink target for StreamAudioPipeline (app wiring). Lifetime: owned by this player.
    [[nodiscard]] WinAudioBackend* audioBackend() noexcept { return &backend_; }

private:
    void syncTrack();
    std::shared_ptr<player::Queue> queue_;
    WinAudioBackend backend_;
    models::PlaybackState st_;
    mutable std::mutex mtx_;
};

} // namespace myytm::platform
