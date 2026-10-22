// Level-dependent gain stages: expander, compressor (WDRC), howl guard, limiter.
// All work sample-by-sample with no look-ahead, so none of them add delay.
#pragma once

#include <algorithm>
#include <cmath>

namespace sunai {

inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
inline float linToDb(float lin) { return 20.0f * std::log10(std::max(lin, 1e-9f)); }

// One-pole smoothing coefficient for a time constant in milliseconds.
inline float coeffForMs(float fs, float ms) {
    return std::exp(-1.0f / (0.001f * ms * fs));
}

// Envelope follower with separate attack/release times.
class Envelope {
public:
    void setup(float fs, float attackMs, float releaseMs) {
        mAtt = coeffForMs(fs, attackMs);
        mRel = coeffForMs(fs, releaseMs);
    }
    inline float process(float x) {
        float a = std::fabs(x);
        float c = a > mEnv ? mAtt : mRel;
        mEnv = c * mEnv + (1.0f - c) * a;
        return mEnv;
    }
    float value() const { return mEnv; }
    void reset() { mEnv = 0.0f; }
private:
    float mAtt = 0.0f, mRel = 0.0f, mEnv = 0.0f;
};

// Wide Dynamic Range Compression: makes soft sounds louder and loud sounds
// only a little louder, so speech is clear without loud noises hurting.
// Below the noise floor it turns into a downward expander so the constant
// hiss of the microphone is not amplified while nobody is talking.
class Wdrc {
public:
    void setup(float fs) {
        mEnv.setup(fs, 5.0f, 80.0f);
        mGainSmooth = coeffForMs(fs, 3.0f);
    }
    // gainDb: overall amplification (the volume buttons).
    // ratio: compression above the knee; 1 = none.
    void setParams(float gainDb, float kneeDb, float ratio, float floorDb) {
        mGainDb = gainDb; mKneeDb = kneeDb; mRatio = std::max(1.0f, ratio); mFloorDb = floorDb;
    }
    // Moves the silence gate (set from the measured background hiss level).
    void setFloor(float floorDb) { mFloorDb = floorDb; }
    // Returns true while the input is below the noise floor (silence).
    bool silent() const { return mSilent; }

    inline float process(float x) {
        float levelDb = linToDb(mEnv.process(x));
        float g = mGainDb;
        if (levelDb > mKneeDb) {
            g -= (levelDb - mKneeDb) * (1.0f - 1.0f / mRatio);
        }
        mSilent = levelDb < mFloorDb;
        if (mSilent) {
            // Downward expansion below the floor: mic hiss and steady background are
            // not amplified (1.5 dB less gain per dB below the floor, up to 24 dB).
            g -= std::min(24.0f, 1.5f * (mFloorDb - levelDb));
        }
        float target = dbToLin(g);
        mGain = mGainSmooth * mGain + (1.0f - mGainSmooth) * target;
        return x * mGain;
    }
    void reset() { mEnv.reset(); mGain = 1.0f; }
private:
    Envelope mEnv;
    float mGainDb = 0.0f, mKneeDb = -40.0f, mRatio = 2.0f, mFloorDb = -62.0f;
    float mGain = 1.0f, mGainSmooth = 0.0f;
    bool mSilent = false;
};

// Howl (acoustic feedback) guard. When sound leaking from the earphone back
// into the phone microphone builds up into a loud, sustained squeal, it pulls
// the gain down quickly and then lets it recover slowly.
class HowlGuard {
public:
    void setup(float fs) {
        mFs = fs;
        mFast.setup(fs, 2.0f, 50.0f);
        mSlow.setup(fs, 400.0f, 400.0f);
        mRecover = dbToLin(6.0f / fs);   // recover 6 dB per second
        mAttack = dbToLin(-60.0f / fs);  // cut up to 60 dB per second
    }
    inline float process(float x) {
        float fast = mFast.process(x);
        float slow = mSlow.process(x);
        // A squeal is loud AND steady: fast and slow envelopes are both high and similar.
        bool howling = slow > kHowlLevel && fast < slow * 1.6f && fast > slow * 0.6f;
        if (howling) {
            mHeld += 1.0f / mFs;
        } else {
            mHeld = std::max(0.0f, mHeld - 2.0f / mFs);
        }
        if (mHeld > 0.25f) {
            mGain = std::max(kMinGain, mGain * mAttack);
        } else {
            mGain = std::min(1.0f, mGain * mRecover);
        }
        return x * mGain;
    }
    float gain() const { return mGain; }
    void reset() { mFast.reset(); mSlow.reset(); mGain = 1.0f; mHeld = 0.0f; }
private:
    static constexpr float kHowlLevel = 0.35f;   // about -9 dBFS sustained
    static constexpr float kMinGain = 0.125f;    // never cut more than 18 dB
    float mFs = 48000.0f;
    Envelope mFast, mSlow;
    float mGain = 1.0f, mHeld = 0.0f, mRecover = 1.0f, mAttack = 1.0f;
};

// Own-voice reduction. The wearer's own voice reaches the phone microphone
// much louder than other people's (it is closer), and with earphone delay it
// comes back as an annoying echo. When the input is louder than a threshold
// (learned from the wearer's voice), the output is turned down quickly and
// held down through the short gaps between words.
class OwnVoiceGate {
public:
    void setup(float fs) {
        mEnv.setup(fs, 1.0f, 40.0f);
        mAttack = coeffForMs(fs, 3.0f);
        mRelease = coeffForMs(fs, 120.0f);
        mHoldSamples = static_cast<int>(0.18f * fs);
    }
    void setParams(bool enabled, float thresholdDb, float duckDb) {
        mEnabled = enabled;
        mThresholdDb = thresholdDb;
        mDuck = dbToLin(duckDb);
    }
    // Feed the raw microphone signal; returns the gain to apply to the output.
    // otherSpeaker = the voice model says this loud voice is NOT the wearer.
    inline float detect(float x, bool otherSpeaker = false) {
        float levelDb = linToDb(mEnv.process(x));
        float target = 1.0f;
        if (mEnabled && otherSpeaker) {
            mHold = 0;   // a loud visitor: let them through
        } else if (mEnabled) {
            if (levelDb > mThresholdDb) { mHold = mHoldSamples; target = mDuck; }
            else if (mHold > 0) { --mHold; target = mDuck; }
        }
        float c = target < mGain ? mAttack : mRelease;
        mGain = c * mGain + (1.0f - c) * target;
        return mGain;
    }
    float levelDb() const { return linToDb(mEnv.value()); }
    bool active() const { return mGain < 0.7f; }
    void reset() { mEnv.reset(); mGain = 1.0f; mHold = 0; }
private:
    Envelope mEnv;
    bool mEnabled = true;
    float mThresholdDb = -26.0f, mDuck = 0.125f;
    float mGain = 1.0f, mAttack = 0.0f, mRelease = 0.0f;
    int mHold = 0, mHoldSamples = 0;
};

// Peak limiter with instant attack: output never goes above the ceiling.
// This is the ear-safety stage.
class Limiter {
public:
    void setup(float fs) { mRelease = coeffForMs(fs, 60.0f); }
    void setCeilingDb(float db) { mCeiling = dbToLin(db); }
    float ceiling() const { return mCeiling; }
    inline float process(float x) {
        float a = std::fabs(x);
        float needed = a > mCeiling ? mCeiling / a : 1.0f;
        if (needed < mGain) mGain = needed;                      // instant attack
        else mGain = mRelease * mGain + (1.0f - mRelease) * needed;  // smooth release
        float y = x * mGain;
        // Final safety clamp in case of rounding.
        return std::max(-mCeiling, std::min(mCeiling, y));
    }
    void reset() { mGain = 1.0f; }
private:
    float mCeiling = 0.5f, mGain = 1.0f, mRelease = 0.0f;
};

}  // namespace sunai
