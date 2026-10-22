#include "Processor.h"

#include <algorithm>
#include <cmath>

namespace sunai {

constexpr float Processor::kBandFreqs[Processor::kBands];

namespace {
// Amplification for each volume step. Real phone mics with no auto-gain
// capture normal speech at 1-2 m around -45 to -55 dBFS, so the steps go
// from +12 dB up to +44 dB; the compressor and limiter keep loud sounds safe.
constexpr float kVolumeDb[6] = {0.0f, 12.0f, 20.0f, 28.0f, 36.0f, 44.0f};
// "Extra loud" for severe hearing loss: more gain and a higher output ceiling.
constexpr float kExtraLoudDb = 10.0f;

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
    mOwnVoice.setup(sampleRate);
    mOwnVoice.reset();
    mFeatures.setup(sampleRate);
    mScene.reset();
    mAppliedPlace = -1;
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
    mBoost[ear][band].store(std::clamp(db, 0.0f, 30.0f));
    mDirty.store(true);
}
void Processor::setBalance(int balance) { mBalance.store(std::clamp(balance, -5, 5)); mDirty.store(true); }
void Processor::setSafety(bool on) { mSafety.store(on); mDirty.store(true); }
void Processor::setSaver(bool on) { mSaver.store(on); mDirty.store(true); }
void Processor::setLowDelay(bool on) { mLowDelay.store(on); mDirty.store(true); }
void Processor::setMuted(bool on) { mMuted.store(on); }
void Processor::setExtraLoud(bool on) { mExtraLoud.store(on); mDirty.store(true); }
void Processor::setOwnVoice(bool on, float thresholdDb) {
    mOwnVoiceOn.store(on);
    mOwnVoiceDb.store(std::clamp(thresholdDb, -50.0f, -6.0f));
    mDirty.store(true);
}

void Processor::setAutoPlace(bool on) { mAuto.store(on); mDirty.store(true); }

void Processor::setVoiceProfile(const float *profile) {
    if (profile) {
        std::copy(profile, profile + kProfile, mPendingProfile);
        mProfileCmd.store(1);
    } else {
        mProfileCmd.store(2);
    }
}
void Processor::startVoiceLearning() { mLearnState.store(1); }
void Processor::stopVoiceLearning() {
    int expected = 1;
    mLearnState.compare_exchange_strong(expected, 2);
}
int Processor::voiceLearningState(float *out) {
    int s = mLearnState.load();
    if (s == 3 && out) std::copy(mLearned, mLearned + kProfile, out);
    return s;
}

void Processor::applySettings() {
    const bool autoPlace = mAuto.load();
    const int place = autoPlace ? mScene.place() : mPlace.load();
    mAppliedPlace = place;
    const PlacePreset &p = kPresets[place];
    const bool saver = mSaver.load();

    mLowCut.setHighPass(mFs, p.lowCutHz);
    mNoise.setEnabled(p.noiseReduction && !saver && !mLowDelay.load());
    // Automatic place selection listens to the AI's speech detector even in
    // places where its noise removal is off (no delay is added for that).
    mNoise.setAnalyse(autoPlace && !saver);
    mFeatures.setPitchEnabled(!saver);
    const bool extra = mExtraLoud.load();
    mWdrc.setParams(kVolumeDb[mVolume.load()] + (extra ? kExtraLoudDb : 0.0f), p.kneeDb,
                    extra ? std::max(p.ratio, 3.0f) : p.ratio, p.floorDb);

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
    // Extra loud raises the ceiling to -3 dBFS (still limited, still protected).
    float ceiling = mSafety.load() ? (extra ? -3.0f : -6.0f) : -1.0f;
    for (auto &l : mLimit) l.setCeilingDb(ceiling);

    mNrLatency.store(mNoise.latencyFrames());
    // Own voice is turned down by 18 dB: still faintly audible, so it feels natural.
    mOwnVoice.setParams(mOwnVoiceOn.load(), mOwnVoiceDb.load(), -18.0f);
}

void Processor::process(const float *in, int inFrames, float *out, int outFrames) {
    if (mDirty.exchange(false)) applySettings();

    const bool muted = mMuted.load();
    const bool saver = mSaver.load();
    bool howl = false;
    bool own = false;
    float inPeakDb = -120.0f;

    // Voice profile changes from the UI.
    int cmd = mProfileCmd.exchange(0);
    if (cmd == 1) {
        mSpeaker.setProfile(mPendingProfile, mPendingProfile + VoiceModel::kDims);
        mHasProfile.store(true);
    } else if (cmd == 2) {
        mSpeaker.clearProfile();
        mHasProfile.store(false);
    }
    int learn = mLearnState.load();
    if (learn == 1 && !mLearnStarted) { mSpeaker.beginLearning(); mLearnStarted = true; }
    const bool autoPlace = mAuto.load();
    const bool needFeatures = autoPlace || learn == 1 || mSpeaker.hasProfile();
    const float loudDb = mOwnVoiceDb.load() - 3.0f;

    for (int i = 0; i < outFrames; ++i) {
        float x = (i < inFrames && !muted) ? in[i] : 0.0f;

        x = mLowCut.process(x);

        // Every 10 ms: voice features for the wearer's voice model and the place detector.
        if (needFeatures && mFeatures.push(x)) {
            if (learn == 1) mSpeaker.learn(mFeatures);
            else mSpeaker.update(mFeatures, mOwnVoice.levelDb() > loudDb);
            if (autoPlace) mScene.push(mFeatures.levelDb(), mNoise.speechProb());
        }

        // Detect own voice on the raw mic level. The gain is applied after the
        // compressor (so the compressor cannot undo it); with AI noise reduction on
        // the audio is 10 ms behind the detector, which acts as a small look-ahead.
        float duck = mOwnVoice.detect(x, mSpeaker.otherSpeaker());
        inPeakDb = std::max(inPeakDb, mOwnVoice.levelDb());
        x = mNoise.process(x);
        x = mWdrc.process(x);
        x = mHowlGuard.process(x);
        x *= duck;
        if (duck < 0.7f) own = true;

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
    mOwnActive.store(own);
    mSpeakerScore.store(mSpeaker.score());

    // Finish voice learning when the UI asks.
    if (learn == 2) {
        float mean[VoiceModel::kDims], var[VoiceModel::kDims];
        if (mSpeaker.finishLearning(mean, var)) {
            std::copy(mean, mean + VoiceModel::kDims, mLearned);
            std::copy(var, var + VoiceModel::kDims, mLearned + VoiceModel::kDims);
            mHasProfile.store(true);
            mLearnState.store(3);
        } else {
            mLearnState.store(4);
        }
        mLearnStarted = false;
    }
    // The AI picked a different place: apply its preset from the next block.
    if (autoPlace && mScene.place() != mAppliedPlace) mDirty.store(true);
    mInLevelDb.store(inPeakDb);
}

}  // namespace sunai
