#include "Processor.h"

#include <algorithm>
#include <cmath>

namespace sunai {

constexpr float Processor::kBandFreqs[Processor::kBands];

namespace {
// Amplification for each volume step. Real phone mics with no auto-gain
// capture normal speech at 1-2 m around -45 to -55 dBFS, so the steps go
// from +10 dB up to +34 dB; the compressor and limiter keep loud sounds safe.
constexpr float kVolumeDb[6] = {0.0f, 10.0f, 16.0f, 22.0f, 28.0f, 34.0f};

struct PlacePreset {
    float lowCutHz;
    bool noiseReduction;
    float kneeDb, ratio, floorDb;
    float speechLiftDb;   // extra lift at 2 and 4 kHz for clarity
};
// Home: quiet room, keep it natural and cheap.
// Talk: one-to-one conversation, AI noise reduction on.
// Crowd: market or bus, stronger low-cut and compression.
constexpr PlacePreset kPresets[3] = {
    {100.0f, false, -42.0f, 2.0f, -72.0f, 0.0f},
    {120.0f, true,  -40.0f, 2.0f, -68.0f, 2.0f},
    {200.0f, true,  -38.0f, 3.0f, -62.0f, 4.0f},
};
}  // namespace

void Processor::prepare(float sampleRate) {
    mFs = sampleRate;
    mNoise.setSampleRate(sampleRate);
    mWdrc.setup(sampleRate);
    mHowlGuard.setup(sampleRate);
    for (auto &l : mLimit) l.setup(sampleRate);
    mTone.setSampleRate(sampleRate);
    mLowCut.reset();
    mWdrc.reset();
    mHowlGuard.reset();
    for (auto &ear : mEq) for (auto &b : ear) b.reset();
    for (auto &l : mLimit) l.reset();
    mDirty.store(true);
}

void Processor::setVolume(int level) { mVolume.store(std::clamp(level, 1, 5)); mDirty.store(true); }
void Processor::setPlace(int place) { mPlace.store(std::clamp(place, 0, 2)); mDirty.store(true); }
void Processor::setBoost(int ear, int band, float db) {
    if (ear < 0 || ear > 1 || band < 0 || band >= kBands) return;
    mBoost[ear][band].store(std::clamp(db, 0.0f, 20.0f));
    mDirty.store(true);
}
void Processor::setBalance(int balance) { mBalance.store(std::clamp(balance, -5, 5)); mDirty.store(true); }
void Processor::setSafety(bool on) { mSafety.store(on); mDirty.store(true); }
void Processor::setSaver(bool on) { mSaver.store(on); mDirty.store(true); }
void Processor::setLowDelay(bool on) { mLowDelay.store(on); mDirty.store(true); }
void Processor::setMuted(bool on) { mMuted.store(on); }

void Processor::applySettings() {
    const PlacePreset &p = kPresets[mPlace.load()];
    const bool saver = mSaver.load();

    mLowCut.setHighPass(mFs, p.lowCutHz);
    mNoise.setEnabled(p.noiseReduction && !saver && !mLowDelay.load());
    mWdrc.setParams(kVolumeDb[mVolume.load()], p.kneeDb, p.ratio, p.floorDb);

    for (int ear = 0; ear < 2; ++ear) {
        for (int b = 0; b < kBands; ++b) {
            // Battery saver keeps only the three bands that matter most for speech.
            bool speechBand = (b >= 2 && b <= 4);
            if (saver && !speechBand) { mEq[ear][b].setBypass(); continue; }
            float db = mBoost[ear][b].load();
            if (b == 3 || b == 4) db += p.speechLiftDb;
            if (b == kBands - 1) mEq[ear][b].setHighShelf(mFs, 6000.0f, db);
            else mEq[ear][b].setPeaking(mFs, kBandFreqs[b], db, 1.0f);
        }
    }

    int bal = mBalance.load();
    mGainL = bal > 0 ? 1.0f - 0.15f * bal : 1.0f;
    mGainR = bal < 0 ? 1.0f + 0.15f * bal : 1.0f;

    // Ear safety: with protection on the output never exceeds -6 dBFS.
    float ceiling = mSafety.load() ? -6.0f : -1.0f;
    for (auto &l : mLimit) l.setCeilingDb(ceiling);

    mNrLatency.store(mNoise.latencyFrames());
}

void Processor::process(const float *in, int inFrames, float *out, int outFrames) {
    if (mDirty.exchange(false)) applySettings();

    const bool muted = mMuted.load();
    const bool saver = mSaver.load();
    bool howl = false;

    for (int i = 0; i < outFrames; ++i) {
        float x = (i < inFrames && !muted) ? in[i] : 0.0f;

        x = mLowCut.process(x);
        x = mNoise.process(x);
        x = mWdrc.process(x);
        x = mHowlGuard.process(x);

        float l = x, r = x;
        // In battery saver, skip the equalizer while nobody is talking.
        if (!(saver && mWdrc.silent())) {
            for (int b = 0; b < kBands; ++b) {
                l = mEq[0][b].process(l);
                r = mEq[1][b].process(r);
            }
        }
        l *= mGainL;
        r *= mGainR;

        float tl, tr;
        if (mTone.next(tl, tr)) { l += tl; r += tr; }

        l = mLimit[0].process(l);
        r = mLimit[1].process(r);
        out[2 * i] = l;
        out[2 * i + 1] = r;

        mMeterAcc += x * x;
        if (mHowlGuard.gain() < 0.9f) howl = true;
    }

    // Sound meter: RMS over ~50 ms, mapped to 0..1 on a -60..0 dB scale.
    mMeterCount += outFrames;
    if (mMeterCount >= static_cast<int>(0.05f * mFs)) {
        float rms = std::sqrt(mMeterAcc / std::max(1, mMeterCount));
        float db = linToDb(rms);
        mMeter = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
        mLevel.store(mMeter);
        mMeterAcc = 0.0f;
        mMeterCount = 0;
    }
    mNrActive.store(mNoise.active());
    mHowl.store(howl);
}

}  // namespace sunai
