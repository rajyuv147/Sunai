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

// A synthetic voiced sound: harmonics of f0 shaped by two "formant" peaks,
// with slight vibrato and a syllable-like loudness pattern.
struct Voice {
    float f0, form1, form2;
    double phase = 0.0;
    float next(int n) {
        float t = n / kFs;
        float f = f0 * (1.0f + 0.02f * std::sin(2.0f * kPi * 5.0f * t));
        phase += f / kFs;
        float s = 0.0f;
        for (int k = 1; k * f0 < 5000.0f; ++k) {
            float fk = k * f0;
            float a = std::exp(-std::pow(std::log(fk / form1) / 0.35f, 2.0f)) +
                      0.6f * std::exp(-std::pow(std::log(fk / form2) / 0.35f, 2.0f)) + 0.03f;
            s += a * std::sin(2.0f * kPi * k * static_cast<float>(phase));
        }
        float syll = 0.65f + 0.35f * std::sin(2.0f * kPi * 4.0f * t);
        return s * syll;
    }
};

// Runs `seconds` of a voice at about `db` dBFS RMS; returns stereo output.
static std::vector<float> runVoice(Processor &p, Voice &v, float db, float seconds, int &clock) {
    // Calibrate the voice's raw RMS once.
    Voice probe = v;
    double acc = 0; int cal = 4800;
    for (int i = 0; i < cal; ++i) { float s = probe.next(i); acc += s * s; }
    float scale = std::pow(10.0f, db / 20.0f) / static_cast<float>(std::sqrt(acc / cal));
    int total = static_cast<int>(seconds * kFs);
    std::vector<float> in(kBlock), out(2 * kBlock), all;
    for (int n = 0; n < total; n += kBlock) {
        for (int i = 0; i < kBlock; ++i) in[i] = scale * v.next(clock++);
        p.process(in.data(), kBlock, out.data(), kBlock);
        all.insert(all.end(), out.begin(), out.end());
    }
    return all;
}

// White noise at about `db` dBFS RMS.
static void runNoise(Processor &p, float db, float seconds) {
    float amp = std::pow(10.0f, db / 20.0f) * 1.732f;
    uint32_t seed = 12345;
    int total = static_cast<int>(seconds * kFs);
    std::vector<float> in(kBlock), out(2 * kBlock);
    for (int n = 0; n < total; n += kBlock) {
        for (int i = 0; i < kBlock; ++i) {
            seed = seed * 1664525u + 1013904223u;
            in[i] = amp * ((seed >> 8) / 8388608.0f - 1.0f);
        }
        p.process(in.data(), kBlock, out.data(), kBlock);
    }
}

int main() {
    std::printf("Sunai sound engine tests\n");
#ifdef SUNAI_RNNOISE_V2
    std::printf("AI noise model: RNNoise 0.2\n");
#else
    std::printf("AI noise model: RNNoise 0.1.1\n");
#endif

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
        // Pure test tones count as noise for the AI, so measure gain with it off.
        Processor p; p.prepare(kFs); p.setPlace(0); p.setSafety(true); p.setLowDelay(true);
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
        std::printf("Extra loud\n");
        Processor n; n.prepare(kFs); n.setPlace(0); n.setVolume(5);
        Processor x; x.prepare(kFs); x.setPlace(0); x.setVolume(5); x.setExtraLoud(true);
        auto a = runSine(n, 1000.0f, -60.0f, 1.0f);
        auto b = runSine(x, 1000.0f, -60.0f, 1.0f);
        std::printf("        very soft input: normal %.1f dBFS, extra loud %.1f dBFS\n",
                    linToDb(rms(a, 0, 24000)), linToDb(rms(b, 0, 24000)));
        CHECK(rms(b, 0, 24000) > rms(a, 0, 24000) * 2.0f, "extra loud is clearly louder");
        auto c = runSine(x, 1000.0f, -3.0f, 1.0f);
        CHECK(peak(c) <= std::pow(10.0f, -3.0f / 20.0f) + 1e-5f, "extra loud still never above -3 dBFS");
    }

    {
        std::printf("Silence gate (background hiss)\n");
        // Steady mic hiss at -68 dBFS must not be amplified to an audible level,
        // while speech-like sound 20 dB above it still is.
        Processor p; p.prepare(kFs); p.setPlace(1); p.setVolume(5);
        runNoise(p, -68.0f, 4.0f);
        std::vector<float> in(kBlock, 0.0f), out(2 * kBlock);
        uint32_t seed = 7; double acc = 0; int n = 0;
        for (int b = 0; b < 500; ++b) {
            for (int i = 0; i < kBlock; ++i) { seed = seed * 1664525u + 1013904223u;
                in[i] = 0.00069f * ((seed >> 8) / 8388608.0f - 1.0f); }
            p.process(in.data(), kBlock, out.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) { acc += out[2 * i] * out[2 * i]; ++n; }
        }
        float hissOut = linToDb(static_cast<float>(std::sqrt(acc / n)));
        std::printf("        hiss at volume 5: %.1f dBFS out (floor measured %.0f dB)\n", hissOut, p.noiseFloorDb());
        CHECK(hissOut < -50.0f, "background hiss stays quiet at full volume");
        Voice v{150.0f, 600.0f, 1800.0f}; int clk = 0;
        auto sp = runVoice(p, v, -45.0f, 1.0f, clk);
        float spOut = linToDb(rms(sp, 0, 24000));
        std::printf("        speech at -45 dBFS in: %.1f dBFS out\n", spOut);
        CHECK(spOut > -25.0f, "speech above the hiss is still strongly amplified");
    }

    {
        std::printf("AI noise reduction delay\n");
        Processor p; p.prepare(kFs);
        p.setPlace(0); runSine(p, 1000, -40, 0.05f);
        CHECK(p.extraLatencyFrames() == 480, "home: AI noise removal on, 10 ms");
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
        std::printf("Learning the wearer's voice\n");
        Processor p; p.prepare(kFs); p.setPlace(0); p.setVolume(2); p.setSafety(false);
        p.setOwnVoice(true, -26.0f);
        Voice wearer{120.0f, 500.0f, 1500.0f};
        Voice visitor{230.0f, 900.0f, 2600.0f};
        int clock = 0;
        p.startVoiceLearning();
        runVoice(p, wearer, -18.0f, 5.0f, clock);
        p.stopVoiceLearning();
        runVoice(p, wearer, -60.0f, 0.02f, clock);
        float profile[Processor::kProfile];
        int state = p.voiceLearningState(profile);
        std::printf("        learned pitch %.0f Hz\n", std::pow(2.0f, profile[0]));
        CHECK(state == 3, "voice learned from 5 s of speech");
        CHECK(std::fabs(std::pow(2.0f, profile[0]) - 120.0f) < 12.0f, "wearer's pitch measured correctly");
        CHECK(p.hasVoiceProfile(), "profile active after learning");

        // Too little voice: only noise during learning.
        Processor q; q.prepare(kFs); q.startVoiceLearning(); runNoise(q, -70.0f, 3.0f);
        q.stopVoiceLearning(); runNoise(q, -70.0f, 0.02f);
        CHECK(q.voiceLearningState(nullptr) == 4, "learning fails cleanly without voice");

        std::printf("Recognising the wearer vs a loud visitor\n");
        Processor off; off.prepare(kFs); off.setPlace(0); off.setVolume(2); off.setSafety(false);
        off.setOwnVoice(false, -26.0f);
        int c2 = 0, c3 = 0;
        // Others talking quietly first, so the "other people" model learns them.
        runVoice(p, visitor, -45.0f, 2.0f, clock);
        // Wearer speaks loudly: turned down.
        auto own = runVoice(p, wearer, -14.0f, 1.0f, clock);
        auto ownRef = runVoice(off, wearer, -14.0f, 1.0f, c2);
        float ownDrop = linToDb(rms(ownRef, 0, 24000)) - linToDb(rms(own, 0, 24000));
        std::printf("        wearer turned down %.1f dB (score %.1f)\n", ownDrop, p.speakerScore());
        CHECK(ownDrop > 12.0f, "wearer's loud voice is turned down");
        // Silence, then a visitor speaks just as loudly: let through.
        runVoice(p, wearer, -80.0f, 0.5f, clock);
        auto vis = runVoice(p, visitor, -14.0f, 1.0f, clock);
        auto visRef = runVoice(off, visitor, -14.0f, 1.0f, c3);
        float visDrop = linToDb(rms(visRef, 0, 24000)) - linToDb(rms(vis, 0, 24000));
        std::printf("        loud visitor turned down only %.1f dB (score %.1f)\n", visDrop, p.speakerScore());
        CHECK(visDrop < 3.0f, "loud visitor is NOT turned down");
        CHECK(allFinite(own) && allFinite(vis), "no NaN with voice model");
    }

    {
        std::printf("Automatic place selection\n");
        Processor p; p.prepare(kFs); p.setAutoPlace(true);
        runNoise(p, -75.0f, 10.0f);
        std::printf("        quiet room: floor %.0f dB -> place %d\n", p.noiseFloorDb(), p.autoPlace());
        CHECK(p.autoPlace() == 0, "quiet room -> Home");
        runNoise(p, -50.0f, 10.0f);
        std::printf("        some noise: floor %.0f dB -> place %d\n", p.noiseFloorDb(), p.autoPlace());
        CHECK(p.autoPlace() == 1, "some background noise -> Talk");
        runNoise(p, -32.0f, 10.0f);
        std::printf("        loud noise: floor %.0f dB -> place %d\n", p.noiseFloorDb(), p.autoPlace());
        CHECK(p.autoPlace() == 2, "loud background -> Crowd");
        CHECK(p.extraLatencyFrames() == 480, "Crowd preset applied (AI noise removal on)");
        runNoise(p, -32.0f, 2.5f);
        runNoise(p, -75.0f, 3.1f);
        CHECK(p.autoPlace() == 2, "one quiet moment does not flip the place");
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
