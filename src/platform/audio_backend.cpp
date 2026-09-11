#include "platform/audio_backend.h"
#include <cmath>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace myytm::platform {

WinAudioBackend::~WinAudioBackend() { stop(); }

bool WinAudioBackend::startTone(int freqHz, int durationMs) {
#ifdef _WIN32
    stop();
    stopRequested_ = false;
    playing_ = true;
    volume_ = std::clamp(volume_, 0, 100);
    // Start thread that streams via waveOut
    thread_ = std::thread([this, freqHz, durationMs]{ toneThread(freqHz); (void)durationMs; });
    return true;
#else
    (void)freqHz; (void)durationMs;
    playing_ = true;
    return true;
#endif
}

void WinAudioBackend::stop() {
    stopRequested_ = true;
    if (thread_.joinable()) thread_.join();
    playing_ = false;
#ifdef _WIN32
    if (hWaveOut_) { waveOutClose(hWaveOut_); hWaveOut_ = nullptr; }
#endif
}

void WinAudioBackend::setVolume(int v) {
    volume_ = std::clamp(v, 0, 100);
#ifdef _WIN32
    if (hWaveOut_) {
        DWORD vol = (DWORD)((volume_ * 0xFFFF / 100) & 0xFFFF);
        DWORD both = (vol | (vol << 16));
        waveOutSetVolume(hWaveOut_, both);
    }
#endif
}

void WinAudioBackend::toneThread(int freqHz) {
#ifdef _WIN32
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = 44100;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.cbSize = 0;

    if (waveOutOpen(&hWaveOut_, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        playing_ = false;
        return;
    }
    setVolume(volume_);

    const int samplesPerBlock = 4410; // 100ms
    const double twoPiF = 2.0 * 3.141592653589793 * freqHz;
    double phase = 0.0;
    double phaseInc = twoPiF / fmt.nSamplesPerSec;

    // Double-buffered streaming
    struct Block { WAVEHDR hdr{}; std::vector<short> data; };
    Block blocks[2];
    for (auto& b : blocks) b.data.resize(samplesPerBlock);

    int idx = 0;
    while (!stopRequested_) {
        Block& b = blocks[idx];
        // Generate sine
        for (int i=0;i<samplesPerBlock;++i) {
            double v = std::sin(phase) * (volume_/100.0) * 16000.0;
            b.data[i] = static_cast<short>(v);
            phase += phaseInc;
            if (phase > 2*3.141592653589793) phase -= 2*3.141592653589793;
        }
        b.hdr.lpData = reinterpret_cast<LPSTR>(b.data.data());
        b.hdr.dwBufferLength = (DWORD)(b.data.size()*sizeof(short));
        b.hdr.dwFlags = 0;
        waveOutPrepareHeader(hWaveOut_, &b.hdr, sizeof(b.hdr));
        waveOutWrite(hWaveOut_, &b.hdr, sizeof(b.hdr));
        // Simple sleep for block duration (100ms) while checking stop
        for (int s=0;s<10 && !stopRequested_; ++s) Sleep(10);
        // Wait for buffer done (poll)
        while (!(b.hdr.dwFlags & WHDR_DONE) && !stopRequested_) Sleep(5);
        waveOutUnprepareHeader(hWaveOut_, &b.hdr, sizeof(b.hdr));
        idx ^= 1;
    }
    waveOutReset(hWaveOut_);
#else
    // Non-Windows: just sleep as placeholder
    while (!stopRequested_) std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
