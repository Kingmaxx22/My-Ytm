#include "platform/audio_backend.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace myytm::platform {

WinAudioBackend::~WinAudioBackend() { stop(); }

void WinAudioBackend::deviceCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount) {
    auto* self = static_cast<WinAudioBackend*>(device->pUserData);
    int16_t* out = static_cast<int16_t*>(output);
    uint32_t ch = device->playback.channels;
    std::lock_guard<std::mutex> lock(self->mutex_);
    for (ma_uint32 f = 0; f < frameCount; ++f) {
        for (uint32_t c = 0; c < ch; ++c) {
            if (self->fifo_.empty()) {
                if (self->loop_ && !self->loopBuf_.empty()) {
                    self->fifo_.insert(self->fifo_.end(), self->loopBuf_.begin(), self->loopBuf_.end());
                } else {
                    *out++ = 0;
                    continue;
                }
            }
            *out++ = self->fifo_.front();
            self->fifo_.pop_front();
        }
    }
}

void WinAudioBackend::stopDeviceLocked() {
    if (deviceInit_) {
        ma_device_stop(&device_);
        ma_device_uninit(&device_);
        deviceInit_ = false;
    }
    fifo_.clear();
    loopBuf_.clear();
    loop_ = false;
}

bool WinAudioBackend::playPcm(const int16_t* samples, size_t sampleCount, uint32_t sampleRateHz,
                               uint16_t channels, bool loop) {
    if (!samples || sampleCount == 0 || sampleRateHz == 0 || channels == 0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
#ifdef _WIN32
    stopDeviceLocked();
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = channels;
    config.sampleRate = sampleRateHz;
    config.dataCallback = &WinAudioBackend::deviceCallback;
    config.pUserData = this;
    if (ma_device_init(nullptr, &config, &device_) != MA_SUCCESS) return false;
    deviceInit_ = true;
    ma_device_set_master_volume(&device_, std::clamp(volume_, 0, 100) / 100.0f);
    fifo_.insert(fifo_.end(), samples, samples + sampleCount);
    if (loop) loopBuf_.assign(samples, samples + sampleCount);
    loop_ = loop;
    if (ma_device_start(&device_) != MA_SUCCESS) {
        stopDeviceLocked();
        return false;
    }
    playing_ = true;
    return true;
#else
    (void)samples; (void)sampleCount; (void)sampleRateHz; (void)channels; (void)loop;
    playing_ = true;
    return true;
#endif
}

bool WinAudioBackend::startTone(int freqHz, int durationMs) {
    // Self-test signal through the real PCM path (legacy placeholder source).
    constexpr uint32_t kRate = 44100;
    int ms = durationMs < 0 ? 1000 : durationMs;
    size_t frames = static_cast<size_t>(kRate) * static_cast<size_t>(ms) / 1000;
    if (frames == 0) frames = 1;
    std::vector<int16_t> pcm(frames);
    const double inc = 2.0 * 3.141592653589793 * freqHz / kRate;
    double phase = 0.0;
    int vol = std::clamp(volume_, 0, 100);
    for (size_t i = 0; i < frames; ++i) {
        pcm[i] = static_cast<int16_t>(std::sin(phase) * (vol / 100.0) * 16000.0);
        phase += inc;
        if (phase > 2 * 3.141592653589793) phase -= 2 * 3.141592653589793;
    }
    return playPcm(pcm.data(), pcm.size(), kRate, 1, durationMs < 0);
}

void WinAudioBackend::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
#ifdef _WIN32
    stopDeviceLocked();
#endif
    playing_ = false;
}

void WinAudioBackend::setVolume(int v) {
    volume_ = std::clamp(v, 0, 100);
    std::lock_guard<std::mutex> lock(mutex_);
#ifdef _WIN32
    if (deviceInit_) ma_device_set_master_volume(&device_, volume_ / 100.0f);
#endif
}

// PlatformAudioPlayer — delegates queue logic, adds real backend tone

PlatformAudioPlayer::PlatformAudioPlayer(std::shared_ptr<player::Queue> q) : queue_(std::move(q)) {
    st_.volume = 50;
}

PlatformAudioPlayer::~PlatformAudioPlayer() { backend_.stop(); }

void PlatformAudioPlayer::syncTrack() {
    const auto* cur = queue_->current();
    if (!cur) {
        st_.hasTrack = false; st_.currentTitle.clear(); st_.currentSubtitle.clear();
        st_.durationSeconds = 0; st_.positionSeconds = 0;
        return;
    }
    st_.hasTrack = true;
    st_.currentTitle = cur->title;
    st_.currentSubtitle = cur->subtitle;
    st_.durationSeconds = (cur->type == models::SearchResultType::Song) ? 210 : 180;
    st_.positionSeconds = std::clamp(st_.positionSeconds, 0, st_.durationSeconds);
}

void PlatformAudioPlayer::play() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_->empty()) return;
    if (!queue_->currentIndex().has_value()) queue_->setCurrent(0);
    syncTrack();
    if (!st_.hasTrack) return;
    st_.status = models::PlaybackStatus::Playing;
    backend_.setVolume(st_.volume);
    backend_.stop();
    backend_.startTone(440);
}

void PlatformAudioPlayer::pause() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (st_.status == models::PlaybackStatus::Playing) {
        st_.status = models::PlaybackStatus::Paused;
        backend_.stop();
    } else if (st_.status == models::PlaybackStatus::Paused) {
        st_.status = models::PlaybackStatus::Playing;
        backend_.startTone(440);
    }
}

void PlatformAudioPlayer::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    st_.status = models::PlaybackStatus::Stopped;
    st_.positionSeconds = 0;
    backend_.stop();
}

void PlatformAudioPlayer::next() {
    std::lock_guard<std::mutex> lock(mtx_);
    backend_.stop();
    if (queue_->next()) {
        syncTrack();
        st_.status = models::PlaybackStatus::Playing;
        st_.positionSeconds = 0;
        backend_.startTone(440);
    } else {
        st_.status = models::PlaybackStatus::Stopped;
        st_.positionSeconds = 0;
    }
}

void PlatformAudioPlayer::previous() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (st_.positionSeconds > 3) { st_.positionSeconds = 0; return; }
    backend_.stop();
    if (queue_->previous()) {
        syncTrack();
        st_.status = models::PlaybackStatus::Playing;
        st_.positionSeconds = 0;
        backend_.startTone(440);
    }
}

void PlatformAudioPlayer::seek(int delta) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!st_.hasTrack) return;
    st_.positionSeconds = std::clamp(st_.positionSeconds + delta, 0, st_.durationSeconds);
}

void PlatformAudioPlayer::setVolume(int v) {
    std::lock_guard<std::mutex> lock(mtx_);
    st_.volume = std::clamp(v, 0, 100);
    backend_.setVolume(st_.volume);
}
void PlatformAudioPlayer::volumeUp(int step) { setVolume(st_.volume + step); }
void PlatformAudioPlayer::volumeDown(int step) { setVolume(st_.volume - step); }

models::PlaybackState PlatformAudioPlayer::state() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return st_;
}
bool PlatformAudioPlayer::isPlaying() const noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    return st_.status == models::PlaybackStatus::Playing;
}

} // namespace myytm::platform
