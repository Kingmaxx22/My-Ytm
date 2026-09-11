#include <iostream>
#include <string>
#include <vector>

#include "models/queue_item.h"
#include "player/audio_decoder.h"
#include "player/mock_player.h"
#include "player/queue.h"
#include "player/stream_pipeline.h"
#include "player/stream_player.h"
#include "youtube/youtube_client.h"

#include <cmath>
#include <cstdint>

// Local deterministic test audio: 16-bit mono WAV with a sine tone.
static std::string makeWav(int sampleRate = 8000, int freqHz = 440, int ms = 100) {
    int frames = sampleRate * ms / 1000;
    std::string out;
    out.reserve(44 + frames * 2);
    auto push32 = [&](uint32_t v) {
        out.push_back(static_cast<char>(v & 0xFF));
        out.push_back(static_cast<char>((v >> 8) & 0xFF));
        out.push_back(static_cast<char>((v >> 16) & 0xFF));
        out.push_back(static_cast<char>((v >> 24) & 0xFF));
    };
    auto push16 = [&](uint16_t v) {
        out.push_back(static_cast<char>(v & 0xFF));
        out.push_back(static_cast<char>((v >> 8) & 0xFF));
    };
    out += "RIFF";
    push32(36 + frames * 2);
    out += "WAVEfmt ";
    push32(16);
    push16(1);
    push16(1);
    push32(sampleRate);
    push32(sampleRate * 2);
    push16(2);
    push16(16);
    out += "data";
    push32(frames * 2);
    const double inc = 2.0 * 3.141592653589793 * freqHz / sampleRate;
    for (int i = 0; i < frames; ++i)
        push16(static_cast<uint16_t>(static_cast<int16_t>(std::sin(inc * i) * 12000.0)));
    return out;
}

using namespace myytm;

static int passed = 0, failed = 0;
#define EXPECT(cond, msg) do { if(cond){++passed; std::cout<<"[PASS] "<<msg<<"\n";} else {++failed; std::cout<<"[FAIL] "<<msg<<" ("#cond")\n";} } while(0)

// Player JSON fixtures -------------------------------------------------------

static std::string kPlayableOpus = R"json({"playabilityStatus":{"status":"OK"},"videoDetails":{"videoId":"VID","lengthSeconds":"200"},"streamingData":{"adaptiveFormats":[
  {"itag":140,"url":"https://example.com/a140.m4a?sig=SECRET_A","mimeType":"audio/mp4; codecs=\"mp4a.40.2\"","bitrate":128000,"audioSampleRate":"44100","audioChannels":2},
  {"itag":251,"url":"https://example.com/a251.webm?sig=SECRET_B","mimeType":"audio/webm; codecs=\"opus\"","bitrate":160000,"audioSampleRate":"48000","audioChannels":2}
]}})json";

static std::string kUnplayable = R"json({"playabilityStatus":{"status":"ERROR","reason":"Video unavailable"}})json";

struct ScriptMock : public youtube::IHttpClient {
    std::vector<youtube::HttpResponse> script;
    size_t calls = 0;
    youtube::HttpRequest lastReq;
    youtube::HttpResponse execute(const youtube::HttpRequest& req) override {
        lastReq = req;
        if (calls < script.size()) return script[calls++];
        return youtube::HttpResponse{500, "", {}, "no more scripted responses"};
    }
};

static std::shared_ptr<player::Queue> makeQueueWithSong(const std::string& id = "VID") {
    auto q = std::make_shared<player::Queue>();
    models::QueueItem item;
    item.id = id;
    item.title = "Song";
    item.subtitle = "Artist";
    item.type = models::SearchResultType::Song;
    q->addToEnd(item);
    return q;
}

static std::shared_ptr<youtube::YouTubeClient> makeClient(std::unique_ptr<youtube::IHttpClient> http) {
    auto c = std::make_shared<youtube::YouTubeClient>(std::move(http));
    youtube::InnertubeConfig cfg;
    c->setInnertubeConfig(cfg);
    return c;
}

// A Player that always fails transport (simulates backend init/playback failure).
struct FailingOutput : public player::Player {
    bool playCalled = false;
    void play() override { playCalled = true; throw std::runtime_error("backend failure"); }
    void pause() override {}
    void stop() override {}
    void next() override {}
    void previous() override {}
    void seek(int) override {}
    void setVolume(int) override {}
    void volumeUp(int = 5) override {}
    void volumeDown(int = 5) override {}
    models::PlaybackState state() const override { return models::PlaybackState{}; }
    bool isPlaying() const noexcept override { return false; }
};

void test_success_handoff() {
    std::cout << "\n-- success handoff --\n";
    auto mock = std::make_unique<ScriptMock>();
    mock->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    auto* raw = mock.get();
    auto q = makeQueueWithSong();
    auto out = std::make_shared<player::MockPlayer>(q);
    auto sp = std::make_shared<player::StreamPlayer>(q, makeClient(std::move(mock)), out);

    std::vector<std::string> logLines;
    sp->setLogSink([&](const std::string& m) { logLines.push_back(m); });

    sp->play();
    EXPECT(sp->isPlaying(), "playing after resolve");
    EXPECT(sp->hasActiveStream(), "active stream set");
    EXPECT(sp->activeVideoId() == "VID", "active videoId");
    EXPECT(sp->activeItag() == 251, "opus 251 selected");
    EXPECT(sp->activeMimeType().find("audio/webm") != std::string::npos, "active mime");
    auto as = sp->activeStream();
    EXPECT(as.has_value() && as->url == "https://example.com/a251.webm?sig=SECRET_B", "URL handed to playback path");
    EXPECT(!sp->hasError(), "no error");
    EXPECT(raw->lastReq.url.find("/youtubei/v1/player") != std::string::npos, "player endpoint used");
    // No sensitive data in logs
    std::string all;
    for (auto& l : logLines) all += l + "\n";
    EXPECT(all.find("SECRET_A") == std::string::npos && all.find("SECRET_B") == std::string::npos, "no stream URL secrets logged");
    EXPECT(all.find("https://example.com/a251") == std::string::npos, "no stream URL logged");
}

void test_missing_unplayable() {
    std::cout << "\n-- missing/unplayable --\n";
    // Unavailable video
    auto mock = std::make_unique<ScriptMock>();
    mock->script.push_back(youtube::HttpResponse{200, kUnplayable, {}, ""});
    auto q = makeQueueWithSong();
    auto out = std::make_shared<player::MockPlayer>(q);
    auto sp = std::make_shared<player::StreamPlayer>(q, makeClient(std::move(mock)), out);
    sp->play();
    EXPECT(!sp->isPlaying(), "not playing when unavailable");
    EXPECT(!sp->hasActiveStream(), "no active stream");
    EXPECT(sp->hasError(), "error recorded");
    EXPECT(sp->lastError().find("unavailable") != std::string::npos, "user-facing reason");

    // Missing selected stream (OK but no audio) — fresh player
    auto mock2 = std::make_unique<ScriptMock>();
    mock2->script.push_back(youtube::HttpResponse{200, R"json({"playabilityStatus":{"status":"OK"}})json", {}, ""});
    auto q2 = makeQueueWithSong();
    auto out2 = std::make_shared<player::MockPlayer>(q2);
    auto sp2 = std::make_shared<player::StreamPlayer>(q2, makeClient(std::move(mock2)), out2);
    sp2->play();
    EXPECT(!sp2->isPlaying() && sp2->hasError(), "missing streams -> error, stopped");

    // Unsupported MIME
    auto mock3 = std::make_unique<ScriptMock>();
    mock3->script.push_back(youtube::HttpResponse{200, R"json({"playabilityStatus":{"status":"OK"},"streamingData":{"adaptiveFormats":[{"itag":999,"url":"https://example.com/x.flac","mimeType":"audio/flac; codecs=\"flac\"","bitrate":900000}]}})json", {}, ""});
    auto q3 = makeQueueWithSong();
    auto out3 = std::make_shared<player::MockPlayer>(q3);
    auto sp3 = std::make_shared<player::StreamPlayer>(q3, makeClient(std::move(mock3)), out3);
    sp3->play();
    EXPECT(!sp3->isPlaying() && sp3->hasError(), "unsupported mime -> error, stopped");
    EXPECT(sp3->lastError().find("audio/flac") != std::string::npos, "mime named in error");
    // Supported set
    EXPECT(player::StreamPlayer::isSupportedMime("audio/webm; codecs=\"opus\""), "webm supported");
    EXPECT(player::StreamPlayer::isSupportedMime("audio/mp4; codecs=\"mp4a.40.2\""), "mp4 supported");
    EXPECT(!player::StreamPlayer::isSupportedMime("video/mp4; codecs=\"avc1\""), "video rejected");
    EXPECT(!player::StreamPlayer::isSupportedMime("audio/flac"), "flac rejected");

    // Non-song queue item is not resolved
    auto mock4 = std::make_unique<ScriptMock>();
    auto* raw4 = mock4.get();
    auto q4 = std::make_shared<player::Queue>();
    models::QueueItem artist;
    artist.id = "UCxyz";
    artist.title = "Artist";
    artist.type = models::SearchResultType::Artist;
    q4->addToEnd(artist);
    auto out4 = std::make_shared<player::MockPlayer>(q4);
    auto sp4 = std::make_shared<player::StreamPlayer>(q4, makeClient(std::move(mock4)), out4);
    sp4->play();
    EXPECT(raw4->calls == 0, "no resolve for non-song");
    EXPECT(!sp4->isPlaying() && sp4->hasError(), "non-song -> error, stopped");

    // Empty queue preserves underlying no-op
    auto q5 = std::make_shared<player::Queue>();
    auto out5 = std::make_shared<player::MockPlayer>(q5);
    auto sp5 = std::make_shared<player::StreamPlayer>(q5, makeClient(std::make_unique<youtube::MockHttpClient>()), out5);
    sp5->play();
    EXPECT(!sp5->isPlaying() && !sp5->hasError(), "empty queue no-op");
}

void test_backend_failure() {
    std::cout << "\n-- backend failure --\n";
    auto mock = std::make_unique<ScriptMock>();
    mock->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    auto q = makeQueueWithSong();
    auto failing = std::make_shared<FailingOutput>();
    auto sp = std::make_shared<player::StreamPlayer>(q, makeClient(std::move(mock)), failing);
    bool threw = false;
    try {
        sp->play();
    } catch (const std::exception&) {
        threw = true;
    }
    EXPECT(threw && failing->playCalled, "backend exception propagates (not swallowed)");
    EXPECT(sp->hasActiveStream(), "resolution still recorded");

    // HTTP-level failure maps cleanly
    auto mockNet = std::make_unique<ScriptMock>();
    mockNet->script.push_back(youtube::HttpResponse{0, "", {}, "connection refused"});
    auto qn = makeQueueWithSong();
    auto outn = std::make_shared<player::MockPlayer>(qn);
    auto spn = std::make_shared<player::StreamPlayer>(qn, makeClient(std::move(mockNet)), outn);
    spn->play();
    EXPECT(!spn->isPlaying() && spn->hasError(), "network failure -> error, stopped");
}

void test_transport_state() {
    std::cout << "\n-- transport state --\n";
    auto mock = std::make_unique<ScriptMock>();
    mock->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    mock->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    auto* raw = mock.get();
    auto q = makeQueueWithSong("VID1");
    models::QueueItem second;
    second.id = "VID2";
    second.title = "Two";
    second.type = models::SearchResultType::Song;
    q->addToEnd(second);
    auto out = std::make_shared<player::MockPlayer>(q);
    auto sp = std::make_shared<player::StreamPlayer>(q, makeClient(std::move(mock)), out);

    sp->play();
    EXPECT(sp->isPlaying() && sp->activeVideoId() == "VID1", "track 1 resolved");
    sp->pause();
    EXPECT(!sp->isPlaying(), "paused");
    EXPECT(sp->hasActiveStream(), "stream kept across pause");
    sp->pause();
    EXPECT(sp->isPlaying(), "resumed");
    sp->next();
    EXPECT(sp->isPlaying() && sp->activeVideoId() == "VID2", "track 2 resolved on next");
    EXPECT(raw->calls == 2, "one resolve per track");
    sp->stop();
    EXPECT(!sp->isPlaying() && !sp->hasActiveStream(), "stop clears stream");
    // volume/seek delegate
    sp->play(); // re-resolves VID2
    sp->setVolume(70);
    EXPECT(out->state().volume == 70, "volume delegates");
    sp->seek(10);
    EXPECT(out->state().positionSeconds == 10, "seek delegates");

    // refreshStream re-resolves without touching transport
    auto mockR = std::make_unique<ScriptMock>();
    mockR->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    auto qr = makeQueueWithSong();
    auto outr = std::make_shared<player::MockPlayer>(qr);
    auto spr = std::make_shared<player::StreamPlayer>(qr, makeClient(std::move(mockR)), outr);
    EXPECT(spr->refreshStream() && spr->hasActiveStream(), "refresh resolves");
    EXPECT(!spr->isPlaying(), "refresh does not start transport");
}

void test_no_secret_logging() {
    std::cout << "\n-- no secret logging --\n";
    auto mock = std::make_unique<ScriptMock>();
    mock->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    mock->script.push_back(youtube::HttpResponse{200, kUnplayable, {}, ""});
    auto q = makeQueueWithSong();
    auto out = std::make_shared<player::MockPlayer>(q);
    auto client = makeClient(std::move(mock));
    client->setAuthHeaderProvider([]() -> std::optional<std::string> { return std::string("Bearer TOKEN_SECRET_XYZ"); });
    auto sp = std::make_shared<player::StreamPlayer>(q, client, out);
    std::vector<std::string> lines;
    sp->setLogSink([&](const std::string& m) { lines.push_back(m); });
    sp->play(); // success
    sp->stop();
    sp->play(); // failure path (unplayable) — exercises error log branch
    std::string all;
    for (auto& l : lines) all += l + "\n";
    EXPECT(all.find("TOKEN_SECRET_XYZ") == std::string::npos, "no token in logs");
    EXPECT(all.find("Bearer") == std::string::npos, "no auth scheme in logs");
    EXPECT(all.find("sig=") == std::string::npos, "no signature params in logs");
    EXPECT(all.find("SECRET_A") == std::string::npos && all.find("SECRET_B") == std::string::npos, "no URL secrets in logs");
    EXPECT(all.find("https://") == std::string::npos, "no URLs in logs");
    EXPECT(!lines.empty(), "events are logged (sanitized)");
}

void test_decoder_wav() {
    std::cout << "\n-- decoder wav --\n";
    std::string wav = makeWav(8000, 440, 100);
    auto r = player::AudioDecoder::decode(wav, "audio/wav");
    EXPECT(r.isOk(), "wav decodes");
    if (r.isOk()) {
        EXPECT(r.value().sampleRateHz == 8000, "rate 8000");
        EXPECT(r.value().channels == 1, "mono");
        EXPECT(r.value().frames() == 800, "800 frames");
        EXPECT(r.value().isValid(), "valid pcm");
        bool anyNonZero = false;
        for (auto s : r.value().samples)
            if (s != 0) { anyNonZero = true; break; }
        EXPECT(anyNonZero, "sine content present");
    }
    // No hint: sniffing alone decodes
    EXPECT(player::AudioDecoder::decode(wav).isOk(), "wav sniff decodes");
}

void test_decoder_unsupported_invalid() {
    std::cout << "\n-- decoder unsupported/invalid --\n";
    EXPECT(player::AudioDecoder::decode("").isErr(), "empty input");
    EXPECT(player::AudioDecoder::decode("not audio at all................").isErr(), "garbage input");
    // WebM magic -> Opus/WebM unsupported
    std::string webm("\x1A\x45\xDF\xA3", 4);
    webm += std::string(64, '\0');
    auto rw = player::AudioDecoder::decode(webm, "audio/webm; codecs=\"opus\"");
    EXPECT(rw.isErr() && rw.error().message.find("Opus") != std::string::npos, "webm -> opus unsupported");
    // MP4 ftyp -> AAC unsupported
    std::string mp4(4, '\0');
    mp4 += "ftypisom";
    mp4 += std::string(64, '\0');
    auto rm = player::AudioDecoder::decode(mp4, "audio/mp4; codecs=\"mp4a.40.2\"");
    EXPECT(rm.isErr() && rm.error().message.find("AAC") != std::string::npos, "mp4 -> aac unsupported");
    // Opus mime hint alone rejects without sniffing content
    auto ro = player::AudioDecoder::decode("OggS..........OpusHead..", "audio/webm; codecs=\"opus\"");
    EXPECT(ro.isErr(), "opus hint -> unsupported");
    EXPECT(player::AudioDecoder::looksLikeWebM(webm), "webm sniff");
    EXPECT(player::AudioDecoder::looksLikeMp4(mp4), "mp4 sniff");
    EXPECT(!player::AudioDecoder::looksLikeWebM(makeWav()), "wav is not webm");
}

void test_pipeline_success_failures() {
    std::cout << "\n-- pipeline --\n";
    std::string wav = makeWav();
    models::AudioStream stream;
    stream.itag = 251;
    stream.url = "https://example.com/a.webm?sig=SECRET_P";
    stream.mimeType = "audio/webm; codecs=\"opus\""; // misleading hint; bytes win
    stream.bitrate = 160000;

    player::PcmAudio got;
    int sinkCalls = 0;
    auto okFetch = [&](const std::string& url) {
        EXPECT(url == stream.url, "fetch receives stream url");
        youtube::HttpResponse r;
        r.statusCode = 200;
        r.body = wav;
        return r;
    };
    player::StreamAudioPipeline pl(okFetch,
                                   [&](const player::PcmAudio& p) { got = p; ++sinkCalls; return true; },
                                   []() {});
    std::vector<std::string> logs;
    pl.setLogSink([&](const std::string& m) { logs.push_back(m); });
    auto ok = pl.playStream(stream);
    EXPECT(ok.isOk(), "pipeline success");
    EXPECT(sinkCalls == 1 && got.sampleRateHz == 8000 && got.frames() == 800, "pcm handed to sink");
    std::string all;
    for (auto& l : logs) all += l + "\n";
    EXPECT(all.find("SECRET_P") == std::string::npos && all.find("https://") == std::string::npos, "no url in pipeline logs");

    // Fetch failure
    player::StreamAudioPipeline plNet([](const std::string&) {
        youtube::HttpResponse r;
        r.statusCode = 0;
        r.errorMessage = "down";
        return r;
    }, [](const player::PcmAudio&) { return true; }, []() {});
    EXPECT(plNet.playStream(stream).isErr(), "transport failure -> err");

    // Expired/invalid: HTTP error status
    player::StreamAudioPipeline pl404([](const std::string&) {
        youtube::HttpResponse r;
        r.statusCode = 403;
        return r;
    }, [](const player::PcmAudio&) { return true; }, []() {});
    auto r404 = pl404.playStream(stream);
    EXPECT(r404.isErr() && r404.error().kind == youtube::ErrorKind::Network, "403 -> network/expired err");

    // Oversize guard
    player::StreamAudioPipeline plBig([&](const std::string&) {
        youtube::HttpResponse r;
        r.statusCode = 200;
        r.body = std::string(static_cast<size_t>(player::StreamAudioPipeline::kMaxFetchBytes) + 1, 'x');
        return r;
    }, [](const player::PcmAudio&) { return true; }, []() {});
    EXPECT(plBig.playStream(stream).isErr(), "oversize -> err");

    // Decode failure (garbage bytes)
    player::StreamAudioPipeline plBad([&](const std::string&) {
        youtube::HttpResponse r;
        r.statusCode = 200;
        r.body = "garbage-bytes-not-audio-..........";
        return r;
    }, [](const player::PcmAudio&) { return true; }, []() {});
    EXPECT(plBad.playStream(stream).isErr(), "undecodable -> err");

    // Output init failure
    player::StreamAudioPipeline plSink([&](const std::string&) {
        youtube::HttpResponse r;
        r.statusCode = 200;
        r.body = wav;
        return r;
    }, [](const player::PcmAudio&) { return false; }, []() {});
    EXPECT(plSink.playStream(stream).isErr(), "sink failure -> err");

    // stop() reaches stop sink
    bool stopped = false;
    player::StreamAudioPipeline plStop(okFetch, [](const player::PcmAudio&) { return true; },
                                       [&]() { stopped = true; });
    plStop.stop();
    EXPECT(stopped, "stop sink called");
}

void test_stream_player_with_pipeline() {
    std::cout << "\n-- stream player + pipeline --\n";
    std::string wav = makeWav();
    auto mock = std::make_unique<ScriptMock>();
    mock->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    auto q = makeQueueWithSong();
    auto out = std::make_shared<player::MockPlayer>(q);
    auto sp = std::make_shared<player::StreamPlayer>(q, makeClient(std::move(mock)), out);

    player::PcmAudio got;
    auto pl = std::make_shared<player::StreamAudioPipeline>(
        [&](const std::string&) {
            youtube::HttpResponse r;
            r.statusCode = 200;
            r.body = wav;
            return r;
        },
        [&](const player::PcmAudio& p) { got = p; return true; }, []() {});
    sp->setStreamPipeline(pl);
    sp->play();
    EXPECT(sp->isPlaying(), "playing with pipeline");
    EXPECT(got.sampleRateHz == 8000 && got.frames() == 800, "decoded pcm reached output");

    // Pipeline decode failure stops output and records error
    auto mock2 = std::make_unique<ScriptMock>();
    mock2->script.push_back(youtube::HttpResponse{200, kPlayableOpus, {}, ""});
    auto q2 = makeQueueWithSong();
    auto out2 = std::make_shared<player::MockPlayer>(q2);
    auto sp2 = std::make_shared<player::StreamPlayer>(q2, makeClient(std::move(mock2)), out2);
    auto pl2 = std::make_shared<player::StreamAudioPipeline>(
        [&](const std::string&) {
            youtube::HttpResponse r;
            r.statusCode = 200;
            r.body = "not audio................";
            return r;
        },
        [](const player::PcmAudio&) { return true; }, []() {});
    sp2->setStreamPipeline(pl2);
    sp2->play();
    EXPECT(!sp2->isPlaying() && sp2->hasError(), "pipeline failure -> stopped + error");
}

int main() {
    test_success_handoff();
    test_missing_unplayable();
    test_backend_failure();
    test_transport_state();
    test_no_secret_logging();
    test_decoder_wav();
    test_decoder_unsupported_invalid();
    test_pipeline_success_failures();
    test_stream_player_with_pipeline();
    std::cout << "\n=== Player Playback Tests: " << passed << " passed, " << failed << " failed ===\n";
    return failed == 0 ? 0 : 1;
}
