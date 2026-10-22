// Checks for the Sunai sound chain. Run from the repo root:
//   cmake -S native-tests -B native-tests/build
//   cmake --build native-tests/build
//   ./native-tests/build/sunai_tests
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Processor.h"

using namespace sunai;

static int gFailures = 0;
#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (cond) std::printf("  ok    %s\n", msg);                        \
        else { std::printf("  FAIL  %s  (%s:%d)\n", msg, __FILE__, __LINE__); ++gFailures; } \
    } while (0)

static constexpr float kFs = 48000.0f;
static constexpr int kBlock = 96;   // 2 ms, a typical low-latency burst
static constexpr float kPi = 3.14159265358979f;

// Runs `seconds` of a sine through the processor; returns stereo output.
static std::vector<float> runSine(Processor &p, float freq, float db, float seconds) {
    int total = static_cast<int>(seconds * kFs);
    std::vector<float> in(kBlock), out(2 * kBlock), all;
    all.reserve(2 * total);
    float amp = std::pow(10.0f, db / 20.0f);
    for (int n = 0; n < total; n += kBlock) {
        for (int i = 0; i < kBlock; ++i) in[i] = amp * std::sin(2.0f * kPi * freq * (n + i) / kFs);
        p.process(in.data(), kBlock, out.data(), kBlock);
        all.insert(all.end(), out.begin(), out.end());
    }
    return all;
}

static float peak(const std::vector<float> &v, int from = 0) {
    float m = 0.0f;
    for (size_t i = from; i < v.size(); ++i) m = std::max(m, std::fabs(v[i]));
    return m;
}

static bool allFinite(const std::vector<float> &v) {
    for (float x : v) if (!std::isfinite(x)) return false;
    return true;
}

// RMS of one channel (0 = left, 1 = right) from sample index `from`.
static float rms(const std::vector<float> &v, int ch, int from = 0) {
    double acc = 0; int n = 0;
    for (size_t i = 2 * from + ch; i < v.size(); i += 2) { acc += v[i] * v[i]; ++n; }
    return n ? static_cast<float>(std::sqrt(acc / n)) : 0.0f;
}

int main() {
    std::printf("Sunai sound engine tests\n");

    {
        std::printf("Silence stays quiet\n");
        Processor p; p.prepare(kFs); p.setVolume(5);
        auto out = runSine(p, 1000.0f, -200.0f, 1.0f);
        CHECK(allFinite(out), "no NaN or infinity");
        CHECK(peak(out) < 1e-4f, "silence in gives silence out");
    }

    {
        std::printf("Ear safety limiter\n");
        Processor p; p.prepare(kFs); p.setVolume(5); p.setSafety(true); p.setPlace(0);
        auto out = runSine(p, 1000.0f, -3.0f, 1.0f);   // very loud input
        CHECK(peak(out) <= std::pow(10.0f, -6.0f / 20.0f) + 1e-5f, "output never above -6 dBFS with protection on");
        p.setSafety(false);
        out = runSine(p, 1000.0f, -3.0f, 1.0f);
        CHECK(peak(out) <= std::pow(10.0f, -1.0f / 20.0f) + 1e-5f, "output never above -1 dBFS with protection off");
    }

    {
        std::printf("Amplification\n");
        Processor p; p.prepare(kFs); p.setPlace(0); p.setSafety(true);
        p.setVolume(1);
        auto quiet = runSine(p, 1000.0f, -45.0f, 1.0f);
        p.setVolume(5);
        auto loud = runSine(p, 1000.0f, -45.0f, 1.0f);
        float rq = rms(quiet, 0, 24000), rl = rms(loud, 0, 24000);
        std::printf("        level 1: %.1f dBFS, level 5: %.1f dBFS\n", linToDb(rq), linToDb(rl));
        CHECK(rl > rq * 4.0f, "volume 5 is much louder than volume 1");
        CHECK(rq > std::pow(10.0f, -45.0f / 20.0f), "soft speech is amplified at level 1");
    }

    {
        std::printf("AI noise reduction delay\n");
        Processor p; p.prepare(kFs);
        p.setPlace(0); runSine(p, 1000, -40, 0.05f);
        CHECK(p.extraLatencyFrames() == 0, "home: no extra delay");
        p.setPlace(1); runSine(p, 1000, -40, 0.05f);
        CHECK(p.extraLatencyFrames() == 480, "talk: exactly 10 ms extra delay");
        p.setSaver(true); runSine(p, 1000, -40, 0.05f);
        CHECK(p.extraLatencyFrames() == 0, "battery saver: AI off, no extra delay");
        p.setSaver(false); p.setLowDelay(true); runSine(p, 1000, -40, 0.05f);
        CHECK(p.extraLatencyFrames() == 0, "low-delay mode: AI off");
        Processor rate; rate.prepare(44100.0f); rate.setPlace(1); runSine(rate, 1000, -40, 0.05f);
        CHECK(rate.extraLatencyFrames() == 0, "44.1 kHz phones: AI safely bypassed");
    }

    {
        std::printf("Ear-test boosts are per ear\n");
        Processor p; p.prepare(kFs); p.setPlace(0); p.setVolume(1);
        p.setBoost(1, 4, 12.0f);  // right ear, 4 kHz, +12 dB
        auto out = runSine(p, 4000.0f, -50.0f, 1.0f);
        float l = rms(out, 0, 24000), r = rms(out, 1, 24000);
        std::printf("        4 kHz left %.1f dB, right %.1f dB\n", linToDb(l), linToDb(r));
        CHECK(r > l * 2.5f, "right ear louder at the boosted band");
    }

    {
        std::printf("Balance\n");
        Processor p; p.prepare(kFs); p.setPlace(0); p.setVolume(2); p.setBalance(4);
        auto out = runSine(p, 1000.0f, -40.0f, 0.5f);
        CHECK(rms(out, 1, 12000) > rms(out, 0, 12000) * 1.5f, "balance to the right lowers the left ear");
    }

    {
        std::printf("Howl guard\n");
        Processor p; p.prepare(kFs); p.setPlace(0); p.setVolume(5); p.setSafety(false);
        auto out = runSine(p, 3000.0f, -20.0f, 2.0f);   // steady loud squeal
        float early = rms(out, 0, 0) , late = rms(out, 0, 72000);
        (void)early;
        std::printf("        squeal level after 1.5 s: %.1f dBFS\n", linToDb(late));
        CHECK(p.howlDetected(), "sustained squeal is detected");
        CHECK(late < std::pow(10.0f, -9.0f / 20.0f), "squeal is pulled down");
    }

    {
        std::printf("Ear-test tones\n");
        Processor p; p.prepare(kFs); p.setMuted(true);
        p.tones().request(1000.0f, 1, -30.0f, 500.0f);
        auto out = runSine(p, 1000.0f, -10.0f, 0.6f);  // mic input must be ignored
        CHECK(rms(out, 0) < 1e-5f, "left ear silent when testing the right ear");
        CHECK(rms(out, 1) > 0.005f, "right ear hears the tone");
        CHECK(!p.tones().playing(), "tone stops after its duration");
        CHECK(std::fabs(out[out.size() - 1]) < 1e-5f, "mic stays muted during the test");
    }

    {
        std::printf("Own-voice reduction\n");
        Processor on; on.prepare(kFs); on.setPlace(0); on.setVolume(2); on.setSafety(false);
        on.setOwnVoice(true, -26.0f);
        Processor off; off.prepare(kFs); off.setPlace(0); off.setVolume(2); off.setSafety(false);
        off.setOwnVoice(false, -26.0f);
        // Own voice: loud at the phone mic (-14 dBFS)
        float loudOn = rms(runSine(on, 300.0f, -14.0f, 0.6f), 0, 4800);
        float loudOff = rms(runSine(off, 300.0f, -14.0f, 0.6f), 0, 4800);
        std::printf("        own voice: %.1f dB lower\n", linToDb(loudOff) - linToDb(loudOn));
        CHECK(loudOn < loudOff * 0.25f, "loud own voice is turned down (>12 dB)");
        CHECK(on.ownVoiceActive(), "own voice is detected");
        // Other people: softer at the mic (-45 dBFS). Run long enough for the hold to end.
        float softOn = rms(runSine(on, 300.0f, -45.0f, 1.0f), 0, 24000);
        float softOff = rms(runSine(off, 300.0f, -45.0f, 1.0f), 0, 24000);
        CHECK(softOn > softOff * 0.9f, "other people's softer speech is not turned down");
        CHECK(on.inputLevelDb() > -50.0f && on.inputLevelDb() < -40.0f, "input level reported for calibration");
    }

    {
        std::printf("Speed\n");
        Processor p; p.prepare(kFs); p.setPlace(2);  // heaviest: crowd with AI
        auto t0 = std::chrono::steady_clock::now();
        auto out = runSine(p, 700.0f, -35.0f, 10.0f);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("        10 s of audio processed in %.0f ms (%.0fx faster than real time)\n", ms, 10000.0 / ms);
        CHECK(allFinite(out), "no NaN in heavy mode");
        CHECK(ms < 10000.0, "runs faster than real time");
    }

    std::printf(gFailures ? "\n%d check(s) FAILED\n" : "\nAll checks passed\n", gFailures);
    return gFailures ? EXIT_FAILURE : EXIT_SUCCESS;
}
