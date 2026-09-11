#pragma once

#include "models/stream_info.h"
#include "player/audio_decoder.h"
#include "player/pcm_audio.h"
#include "youtube/http_client.h"
#include "youtube/result.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace myytm::player {

// Fetches a resolved AudioStream URL, decodes it to PCM, and hands PCM to an
// output sink. Keeps StreamPlayer free of HTTP/decode details.
//
// Security: stream URLs are held in memory only and NEVER logged. The log
// sink receives sanitized messages (itag, mime, byte counts, user-facing
// reasons) — never URLs, tokens, keys, or cookies.
class StreamAudioPipeline {
public:
    using FetchFn = std::function<youtube::HttpResponse(const std::string& url)>;
    using PcmSink = std::function<bool(const PcmAudio&)>;
    using StopSink = std::function<void()>;

    StreamAudioPipeline(FetchFn fetcher, PcmSink pcmSink, StopSink stopSink);

    // Full handoff: fetch -> size guard -> decode -> PCM sink.
    [[nodiscard]] youtube::Result<void> playStream(const models::AudioStream& stream);
    void stop();

    void setLogSink(std::function<void(const std::string&)> sink);

    static constexpr long long kMaxFetchBytes = 64LL * 1024LL * 1024LL;

private:
    void logSanitized(const std::string& msg) const;

    FetchFn fetcher_;
    PcmSink pcmSink_;
    StopSink stopSink_;
    std::function<void(const std::string&)> logSink_;
    mutable std::mutex mtx_;
};

} // namespace myytm::player
