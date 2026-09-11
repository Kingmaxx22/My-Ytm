#pragma once

#include "models/stream_info.h"
#include "player/player.h"
#include "player/queue.h"
#include "player/stream_pipeline.h"
#include "youtube/youtube_client.h"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace myytm::player {

// Bridges InnerTube playback resolution to the audio output path.
//
// Flow: play()/next()/previous() resolve the current queue item's videoId via
// YouTubeClient::resolvePlayback. When resolution yields a usable stream with a
// supported audio MIME, the selected stream is stored as the active stream and
// the output player is started. On any failure the output is stopped and a
// user-facing error is recorded.
//
// Security: the active stream URL is held in memory only and NEVER logged.
// The optional log sink receives sanitized messages only (videoId, itag,
// mime type, user-facing reason). Tokens, API keys, cookies and URLs must
// never be passed to it.
//
// Transport (pause/stop/seek/volume/queue navigation) delegates to the output
// player unchanged. Resolution is synchronous today; a future revision should
// move it off the UI thread.
class StreamPlayer final : public Player {
public:
    StreamPlayer(std::shared_ptr<Queue> queue,
                 std::shared_ptr<youtube::YouTubeClient> resolver,
                 std::shared_ptr<Player> output);

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
    [[nodiscard]] bool isPlaying() const noexcept override;

    // Resolution state — safe metadata only (no URL).
    [[nodiscard]] bool hasActiveStream() const;
    [[nodiscard]] std::string activeVideoId() const;
    [[nodiscard]] std::string activeMimeType() const;
    [[nodiscard]] int activeItag() const;
    // Handoff point for the audio backend: the selected stream to consume.
    // Returns nullopt when nothing is resolved.
    [[nodiscard]] std::optional<models::AudioStream> activeStream() const;
    [[nodiscard]] bool hasError() const;
    [[nodiscard]] std::string lastError() const; // user-facing, safe to display

    // Re-resolve the current track without touching transport state.
    // Used after an expired/invalid stream URL is detected downstream.
    // Returns true when a usable stream is active afterwards.
    bool refreshStream();

    // Receives sanitized event messages only. Never receives URLs/tokens/keys.
    void setLogSink(std::function<void(const std::string&)> sink);

    // Optional real-audio path. When set, a resolved stream is fetched,
    // decoded to PCM and handed to the pipeline's output sink after the output
    // player is started (the sink takes over the backend). Pipeline failures
    // stop the output and surface as lastError(). Unset = legacy output-only.
    void setStreamPipeline(std::shared_ptr<StreamAudioPipeline> pipeline);

    // MIME allowlist for handoff: formats a stream decoder can consume.
    // The bundled WinMM backend is tone-only (see audio_backend.h), so these
    // formats are handed off as resolved metadata pending a decoder backend.
    [[nodiscard]] static bool isSupportedMime(const std::string& mimeType) noexcept;

private:
    // Resolve current queue item; updates active_/lastError_. Caller holds mtx_.
    // Returns true when a usable, supported stream is active.
    bool resolveCurrentLocked();
    // Run the pipeline for the active stream (no-op when unset).
    // Returns false and records lastError_ on pipeline failure. Caller holds mtx_.
    bool runPipelineLocked();
    void logSanitized(const std::string& msg) const;

    std::shared_ptr<Queue> queue_;
    std::shared_ptr<youtube::YouTubeClient> resolver_;
    std::shared_ptr<Player> output_;

    std::optional<models::AudioStream> active_; // holds the URL; never logged
    std::string activeVideoId_;
    std::shared_ptr<StreamAudioPipeline> pipeline_;
    std::string lastError_;
    std::function<void(const std::string&)> logSink_;
    mutable std::mutex mtx_;
};

} // namespace myytm::player
