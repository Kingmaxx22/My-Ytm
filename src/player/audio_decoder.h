#pragma once

#include "player/pcm_audio.h"
#include "youtube/result.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace myytm::player {

// Decodes compressed audio bytes to PCM using miniaudio (WAV, MP3, FLAC,
// Vorbis-in-Ogg). Opus-in-WebM and AAC-in-MP4 — the formats YouTube Music
// serves — are NOT decodable by miniaudio and report a clean unsupported
// error (see README note in report). No network, no logging of input bytes.
class AudioDecoder {
public:
    // mimeHint comes from AudioStream.mimeType (may be empty for sniff-only).
    // Never logs input data.
    [[nodiscard]] static youtube::Result<PcmAudio> decode(const std::string& bytes,
                                                          std::string_view mimeHint = {});

    // Container/codec sniffing for clear error messages.
    [[nodiscard]] static bool looksLikeWebM(const std::string& bytes) noexcept;
    [[nodiscard]] static bool looksLikeMp4(const std::string& bytes) noexcept;
    [[nodiscard]] static bool looksLikeOpus(const std::string& bytes, std::string_view mimeHint) noexcept;
    // True when bytes positively identify a miniaudio-decodable format
    // (WAV/MP3/FLAC/Vorbis-in-Ogg). Wins over a misleading mime hint.
    [[nodiscard]] static bool looksDecodable(const std::string& bytes) noexcept;

    static constexpr size_t kMaxInputBytes = 128u * 1024u * 1024u; // decoder guard
};

} // namespace myytm::player
