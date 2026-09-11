#pragma once

#include <cstdint>
#include <vector>

namespace myytm::player {

// Decoded PCM audio: interleaved signed 16-bit samples.
struct PcmAudio {
    std::vector<int16_t> samples; // interleaved, size = frames * channels
    uint32_t sampleRateHz = 0;
    uint16_t channels = 0;

    [[nodiscard]] bool empty() const noexcept { return samples.empty(); }
    [[nodiscard]] size_t frames() const noexcept {
        return channels == 0 ? 0 : samples.size() / channels;
    }
    [[nodiscard]] bool isValid() const noexcept {
        return sampleRateHz > 0 && channels > 0 && !samples.empty() &&
               samples.size() % channels == 0;
    }
};

} // namespace myytm::player
