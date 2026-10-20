// The complete live sound chain, mono microphone in, stereo earphone out:
//
//   mic -> low-cut -> AI noise reduction -> WDRC compressor/expander
//       -> howl guard -> per-ear equalizer (from the ear test) -> balance
//       -> safety limiter -> earphone
//
// Settings are written from the UI thread (atomics) and picked up by the
// audio thread at the start of the next block, so the UI never blocks audio.
#pragma once

#include <atomic>
#include <cstdint>

#include "Biquad.h"
#include "Dynamics.h"
#include "NoiseReducer.h"
#include "ToneGenerator.h"

namespace sunai {

enum class Place : int { Home = 0, Talk = 1, Crowd = 2 };

class Processor {
public:
    static constexpr int kBands = 6;
    static constexpr float kBandFreqs[kBands] = {250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f};

    void prepare(float sampleRate);

    // ---- Settings (any thread) ----
    void setVolume(int level);                 // 1..5
    void setPlace(int place);                  // Place
    void setBoost(int ear, int band, float db); // ear 0 = left, 1 = right
    void setBalance(int balance);              // -5 (left) .. +5 (right)
    void setSafety(bool on);                   // lower output ceiling
    void setSaver(bool on);                    // battery saver
    void setLowDelay(bool on);                 // skip the 10 ms AI stage
    void setMuted(bool on);                    // mic path silent (e.g. during the ear test)

    ToneGenerator &tones() { return mTone; }

    // ---- Status (any thread) ----
    float level() const { return mLevel.load(); }          // 0..1 for the sound meter
    bool noiseReductionActive() const { return mNrActive.load(); }
    bool howlDetected() const { return mHowl.load(); }
    int extraLatencyFrames() const { return mNrLatency.load(); }

    // ---- Audio thread ----
    // `in` holds `inFrames` mono samples (may be fewer than outFrames if the
    // microphone was late; the rest is treated as silence).
    // `out` receives `outFrames` interleaved stereo samples.
    void process(const float *in, int inFrames, float *out, int outFrames);

private:
    void applySettings();

    float mFs = 48000.0f;

    // UI-written settings
    std::atomic<int> mVolume{3}, mPlace{1}, mBalance{0};
    std::atomic<bool> mSafety{true}, mSaver{false}, mLowDelay{false}, mMuted{false};
    std::atomic<float> mBoost[2][kBands] = {};
    std::atomic<bool> mDirty{true};

    // Status
    std::atomic<float> mLevel{0.0f};
    std::atomic<bool> mNrActive{false}, mHowl{false};
    std::atomic<int> mNrLatency{0};

    // DSP blocks (audio thread only)
    Biquad mLowCut;
    NoiseReducer mNoise;
    Wdrc mWdrc;
    HowlGuard mHowlGuard;
    Biquad mEq[2][kBands];
    Limiter mLimit[2];
    ToneGenerator mTone;
    float mGainL = 1.0f, mGainR = 1.0f;
    float mMeterAcc = 0.0f;
    int mMeterCount = 0;
    float mMeter = 0.0f;
};

}  // namespace sunai
