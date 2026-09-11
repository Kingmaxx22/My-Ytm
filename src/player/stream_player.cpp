#include "player/stream_player.h"

#include <algorithm>
#include <cctype>

namespace myytm::player {

namespace {

std::string toLowerCopy(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

} // namespace

StreamPlayer::StreamPlayer(std::shared_ptr<Queue> queue,
                           std::shared_ptr<youtube::YouTubeClient> resolver,
                           std::shared_ptr<Player> output)
    : queue_(std::move(queue)), resolver_(std::move(resolver)), output_(std::move(output)) {}

bool StreamPlayer::isSupportedMime(const std::string& mimeType) noexcept {
    std::string m;
    m.reserve(mimeType.size());
    for (char c : mimeType) m.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return m.rfind("audio/webm", 0) == 0 || m.rfind("audio/mp4", 0) == 0;
}

void StreamPlayer::logSanitized(const std::string& msg) const {
    if (logSink_) logSink_(msg);
}

void StreamPlayer::setLogSink(std::function<void(const std::string&)> sink) {
    std::lock_guard<std::mutex> lock(mtx_);
    logSink_ = std::move(sink);
}

void StreamPlayer::setStreamPipeline(std::shared_ptr<StreamAudioPipeline> pipeline) {
    std::lock_guard<std::mutex> lock(mtx_);
    pipeline_ = std::move(pipeline);
}

bool StreamPlayer::runPipelineLocked() {
    if (!pipeline_ || !active_) return true;
    auto pr = pipeline_->playStream(*active_);
    if (pr.isErr()) {
        lastError_ = pr.error().message; // user-facing, secret-free by contract
        logSanitized("Playback error: " + lastError_);
        return false;
    }
    return true;
}

bool StreamPlayer::resolveCurrentLocked() {
    active_.reset();
    activeVideoId_.clear();
    lastError_.clear();

    if (!queue_ || !resolver_) {
        lastError_ = "Playback is not configured.";
        logSanitized("Playback error: resolver unavailable");
        return false;
    }
    const models::QueueItem* cur = queue_->current();
    if (!cur) {
        lastError_ = "Nothing to play.";
        return false;
    }
    if (cur->id.empty() || cur->type != models::SearchResultType::Song) {
        lastError_ = "Only songs can be played.";
        logSanitized("Playback error: unsupported queue item type");
        return false;
    }
    auto res = resolver_->resolvePlayback(cur->id);
    if (res.isErr()) {
        lastError_ = res.error().message; // user-facing, secret-free by contract
        logSanitized("Playback error: " + lastError_);
        return false;
    }
    const models::PlaybackResolution& r = res.value();
    if (!r.hasPlayableStream()) {
        lastError_ = "No playable audio streams for this video.";
        logSanitized("Playback error: no playable audio stream (videoId=" + r.videoId + ")");
        return false;
    }
    const models::AudioStream& s = *r.selectedStream;
    if (!isSupportedMime(s.mimeType)) {
        lastError_ = "Unsupported audio format (" + s.mimeType + ").";
        logSanitized("Playback error: unsupported mime (videoId=" + r.videoId + ")");
        return false;
    }
    active_ = s; // URL stored in memory only — never logged
    activeVideoId_ = r.videoId;
    logSanitized("Stream resolved (videoId=" + r.videoId + " itag=" + std::to_string(s.itag) +
                 " mime=" + s.mimeType + ")");
    return true;
}

void StreamPlayer::play() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!queue_ || queue_->empty()) {
        if (output_) output_->play(); // preserves underlying no-op
        return;
    }
    if (!queue_->currentIndex().has_value()) queue_->setCurrent(0);
    if (resolveCurrentLocked()) {
        output_->play(); // sync transport state first...
        if (!runPipelineLocked()) output_->stop(); // ...then PCM takes over the backend
    } else {
        output_->stop(); // never leave stale audio running on failure
    }
}

void StreamPlayer::pause() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (output_) output_->pause();
}

void StreamPlayer::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    active_.reset();
    activeVideoId_.clear();
    if (output_) output_->stop();
}

void StreamPlayer::next() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!output_ || !queue_ || queue_->empty()) return;
    output_->next();
    if (queue_->empty()) {
        active_.reset();
        activeVideoId_.clear();
        lastError_.clear();
        return;
    }
    if (!output_->isPlaying()) {
        // Transport stopped (e.g. end of queue): no stale stream, no error.
        active_.reset();
        activeVideoId_.clear();
        lastError_.clear();
        return;
    }
    const models::QueueItem* cur = queue_->current();
    if (cur && cur->id == activeVideoId_ && active_.has_value()) return; // same track
    if (resolveCurrentLocked()) {
        if (!runPipelineLocked()) output_->stop();
    } else {
        output_->stop();
    }
}

void StreamPlayer::previous() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!output_ || !queue_ || queue_->empty()) return;
    std::string beforeId = queue_->current() ? queue_->current()->id : std::string{};
    output_->previous();
    if (queue_->empty() || !output_->isPlaying()) {
        active_.reset();
        activeVideoId_.clear();
        if (!output_->isPlaying()) lastError_.clear();
        return;
    }
    const models::QueueItem* cur = queue_->current();
    if (cur && cur->id == beforeId && cur->id == activeVideoId_ && active_.has_value()) return;
    if (resolveCurrentLocked()) {
        if (!runPipelineLocked()) output_->stop();
    } else {
        output_->stop();
    }
}

void StreamPlayer::seek(int deltaSeconds) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (output_) output_->seek(deltaSeconds);
}

void StreamPlayer::setVolume(int volume) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (output_) output_->setVolume(volume);
}

void StreamPlayer::volumeUp(int step) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (output_) output_->volumeUp(step);
}

void StreamPlayer::volumeDown(int step) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (output_) output_->volumeDown(step);
}

models::PlaybackState StreamPlayer::state() const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (output_) return output_->state();
    return models::PlaybackState{};
}

bool StreamPlayer::isPlaying() const noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    return output_ && output_->isPlaying();
}

bool StreamPlayer::hasActiveStream() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return active_.has_value();
}

std::string StreamPlayer::activeVideoId() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return activeVideoId_;
}

std::string StreamPlayer::activeMimeType() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return active_ ? active_->mimeType : std::string{};
}

int StreamPlayer::activeItag() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return active_ ? active_->itag : 0;
}

std::optional<models::AudioStream> StreamPlayer::activeStream() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return active_;
}

bool StreamPlayer::hasError() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return !lastError_.empty();
}

std::string StreamPlayer::lastError() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return lastError_;
}

bool StreamPlayer::refreshStream() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!queue_ || queue_->empty() || !queue_->current()) {
        lastError_ = "Nothing to play.";
        return false;
    }
    return resolveCurrentLocked(); // transport untouched by design
}

} // namespace myytm::player
