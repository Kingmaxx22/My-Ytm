#pragma once
#include "player/player.h"
#include "player/queue.h"
#include "models/playback_state.h"
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace myytm::platform {

// Real audio backend — isolated in platform layer per AGENTS.md.
// Uses WinMM waveOut to emit a demo tone (440Hz sine) when playing.
// This proves the Player→Platform→Audio chain without requiring YouTube stream decipher
// (which would violate ToS). Swapping to miniaudio/FFmpeg decoders only changes this file.
class WinAudioBackend {
public:
    WinAudioBackend() = default;
    ~WinAudioBackend();

    bool startTone(int freqHz = 440, int durationMs = -1); // -1 = indefinite until stop
    void stop();
    void setVolume(int vol01); // 0-100
    [[nodiscard]] bool isPlaying() const noexcept { return playing_; }

private:
    void toneThread(int freqHz);
    std::atomic<bool> playing_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread thread_;
    std::mutex mutex_;
    int volume_ = 50;
#ifdef _WIN32
    HWAVEOUT hWaveOut_ = nullptr;
#endif
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

private:
    void syncTrack();
    std::shared_ptr<player::Queue> queue_;
    WinAudioBackend backend_;
    models::PlaybackState st_;
    mutable std::mutex mtx_;
};

} // namespace myytm::platform
