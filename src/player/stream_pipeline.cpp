#include "player/stream_pipeline.h"

namespace myytm::player {

StreamAudioPipeline::StreamAudioPipeline(FetchFn fetcher, PcmSink pcmSink, StopSink stopSink)
    : fetcher_(std::move(fetcher)), pcmSink_(std::move(pcmSink)), stopSink_(std::move(stopSink)) {}

void StreamAudioPipeline::logSanitized(const std::string& msg) const {
    if (logSink_) logSink_(msg);
}

void StreamAudioPipeline::setLogSink(std::function<void(const std::string&)> sink) {
    std::lock_guard<std::mutex> lock(mtx_);
    logSink_ = std::move(sink);
}

youtube::Result<void> StreamAudioPipeline::playStream(const models::AudioStream& stream) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (stream.url.empty())
        return youtube::Result<void>::err(youtube::Error::parse("Missing stream URL."));
    if (!fetcher_ || !pcmSink_)
        return youtube::Result<void>::err(youtube::Error::parse("Audio playback is not configured."));

    youtube::HttpResponse resp = fetcher_(stream.url); // URL in memory only, never logged
    if (!resp.errorMessage.empty() && resp.statusCode == 0)
        return youtube::Result<void>::err(
            youtube::Error::network("Unable to fetch audio. Check your connection and try again."));
    if (!resp.isSuccess())
        return youtube::Result<void>::err(youtube::Error{
            youtube::ErrorKind::Network,
            "Audio stream request failed (" + std::to_string(resp.statusCode) + "). It may have expired.",
            resp.statusCode});
    if (resp.body.empty())
        return youtube::Result<void>::err(youtube::Error::parse("Audio stream is empty. It may have expired."));
    if (static_cast<long long>(resp.body.size()) > kMaxFetchBytes)
        return youtube::Result<void>::err(youtube::Error::parse("Audio stream too large to play."));

    auto decoded = AudioDecoder::decode(resp.body, stream.mimeType);
    if (decoded.isErr()) return youtube::Result<void>::err(decoded.error());

    if (!pcmSink_(decoded.value())) {
        logSanitized("Audio output failed (itag=" + std::to_string(stream.itag) + ")");
        return youtube::Result<void>::err(youtube::Error::parse("Audio output failed."));
    }
    logSanitized("Playing stream (itag=" + std::to_string(stream.itag) + " mime=" + stream.mimeType +
                 " bytes=" + std::to_string(resp.body.size()) + ")");
    return youtube::Result<void>::ok();
}

void StreamAudioPipeline::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (stopSink_) stopSink_();
}

} // namespace myytm::player
