#pragma once

#include <optional>
#include <string>
#include <vector>

namespace myytm::models {

// Usable audio-only stream from InnerTube streamingData.
// URL may contain expiring signatures — treat as transient, never log.
struct AudioStream {
    int itag = 0;
    std::string url;
    std::string mimeType; // e.g. "audio/webm; codecs=\"opus\""
    std::string codecs; // extracted from mimeType, may be empty
    long long bitrate = 0;
    int sampleRateHz = 0;
    int channels = 0;
    long long contentLengthBytes = 0; // 0 if unknown
    long long approxDurationMs = 0; // 0 if unknown
    std::string audioQuality; // e.g. "AUDIO_QUALITY_HIGH"
    bool isDefaultTrack = true;

    [[nodiscard]] bool isAudioOnly() const noexcept
    {
        return mimeType.rfind("audio/", 0) == 0;
    }
    [[nodiscard]] bool isUsable() const noexcept { return !url.empty() && isAudioOnly(); }
};

enum class Playability {
    Playable,
    LoginRequired, // private / needs sign-in
    AgeRestricted, // age-gated
    Unavailable, // deleted / blocked / region-restricted / error status
};

// First real playback-resolution layer: videoId -> stream info.
// No audio backend coupling — the player consumes selectedStream later.
struct PlaybackResolution {
    std::string videoId;
    Playability playability = Playability::Unavailable;
    std::string playabilityReason; // user-facing message from YouTube, safe to display
    std::vector<AudioStream> audioStreams; // all usable audio-only streams
    std::optional<AudioStream> selectedStream; // best pick, set when playable
    std::optional<long long> durationMs;

    [[nodiscard]] bool isPlayable() const noexcept { return playability == Playability::Playable; }
    [[nodiscard]] bool hasPlayableStream() const noexcept
    {
        return isPlayable() && selectedStream.has_value() && selectedStream->isUsable();
    }
};

} // namespace myytm::models
