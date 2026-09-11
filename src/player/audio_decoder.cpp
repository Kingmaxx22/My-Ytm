#include "player/audio_decoder.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include "miniaudio.h"

namespace myytm::player {

bool AudioDecoder::looksLikeWebM(const std::string& bytes) noexcept {
    // EBML header 0x1A 0x45 0xDF 0xA3
    return bytes.size() >= 4 &&
           static_cast<unsigned char>(bytes[0]) == 0x1A &&
           static_cast<unsigned char>(bytes[1]) == 0x45 &&
           static_cast<unsigned char>(bytes[2]) == 0xDF &&
           static_cast<unsigned char>(bytes[3]) == 0xA3;
}

bool AudioDecoder::looksLikeMp4(const std::string& bytes) noexcept {
    // ....ftyp
    return bytes.size() >= 8 && std::memcmp(bytes.data() + 4, "ftyp", 4) == 0;
}

bool AudioDecoder::looksDecodable(const std::string& bytes) noexcept {
    if (bytes.size() < 4) return false;
    const auto* b = reinterpret_cast<const unsigned char*>(bytes.data());
    // WAV: RIFF....WAVE
    if (bytes.size() >= 12 && std::memcmp(b, "RIFF", 4) == 0 && std::memcmp(b + 8, "WAVE", 4) == 0)
        return true;
    // MP3: ID3 tag or MPEG frame sync 0xFFE
    if (std::memcmp(b, "ID3", 3) == 0) return true;
    if (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0) return true;
    // FLAC: fLaC
    if (std::memcmp(b, "fLaC", 4) == 0) return true;
    // Ogg Vorbis: OggS without OpusHead (Opus-in-Ogg stays unsupported)
    if (std::memcmp(b, "OggS", 4) == 0 && bytes.find("OpusHead") == std::string::npos) return true;
    return false;
}

bool AudioDecoder::looksLikeOpus(const std::string& bytes, std::string_view mimeHint) noexcept {
    std::string m(mimeHint);
    std::transform(m.begin(), m.end(), m.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (m.find("opus") != std::string::npos) return true;
    // OpusHead magic inside Ogg-wrapped Opus
    if (bytes.size() >= 4 && std::memcmp(bytes.data(), "OggS", 4) == 0 &&
        bytes.find("OpusHead") != std::string::npos)
        return true;
    return false;
}

youtube::Result<PcmAudio> AudioDecoder::decode(const std::string& bytes, std::string_view mimeHint) {
    if (bytes.empty())
        return youtube::Result<PcmAudio>::err(youtube::Error::parse("Empty audio data."));
    if (bytes.size() > kMaxInputBytes)
        return youtube::Result<PcmAudio>::err(youtube::Error::parse("Audio data too large to decode."));
    // Positively-identified decodable bytes win over a misleading mime hint
    // (e.g. a test WAV served with an opus mime type).
    bool decodable = looksDecodable(bytes);
    if (!decodable && (looksLikeWebM(bytes) || looksLikeOpus(bytes, mimeHint)))
        return youtube::Result<PcmAudio>::err(youtube::Error::parse(
            "Unsupported audio format (Opus/WebM needs an Opus decoder, not bundled)."));
    if (!decodable && looksLikeMp4(bytes)) {
        std::string m(mimeHint);
        std::transform(m.begin(), m.end(), m.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (m.find("video/") != std::string::npos)
            return youtube::Result<PcmAudio>::err(
                youtube::Error::parse("Unsupported audio format (not an audio stream)."));
        if (m.find("mp4") != std::string::npos || m.find("mp4a") != std::string::npos ||
            m.find("audio/") != std::string::npos || m.empty())
            return youtube::Result<PcmAudio>::err(youtube::Error::parse(
                "Unsupported audio format (AAC/MP4 needs an AAC decoder, not bundled)."));
    }

    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 0, 0);
    ma_decoder decoder;
    ma_result r = ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder);
    if (r != MA_SUCCESS)
        return youtube::Result<PcmAudio>::err(youtube::Error::parse("Unrecognized audio data."));

    PcmAudio out;
    out.sampleRateHz = decoder.outputSampleRate;
    out.channels = static_cast<uint16_t>(decoder.outputChannels);

    constexpr ma_uint64 kChunkFrames = 4096;
    std::vector<int16_t> chunk(static_cast<size_t>(kChunkFrames) * (out.channels ? out.channels : 2));
    while (true) {
        ma_uint64 framesRead = 0;
        r = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames, &framesRead);
        if (framesRead > 0)
            out.samples.insert(out.samples.end(), chunk.data(),
                               chunk.data() + static_cast<size_t>(framesRead) * out.channels);
        if (r != MA_SUCCESS) break;
    }
    ma_decoder_uninit(&decoder);

    if (!out.isValid())
        return youtube::Result<PcmAudio>::err(youtube::Error::parse("Decoded audio is empty or invalid."));
    return youtube::Result<PcmAudio>::ok(std::move(out));
}

} // namespace myytm::player
