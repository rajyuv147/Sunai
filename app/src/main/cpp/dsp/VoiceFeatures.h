// Small on-device voice model ("learn my voice").
//
// Every 10 ms it measures:
//   - pitch (how high or low the voice is), by autocorrelation
//   - the shape of the spectrum in 8 bands (the "colour" of the voice),
//     normalised so loudness does not matter
// The wearer's voice is learned as a Gaussian model from ~5 s of their speech.
// A second "other people" model learns continuously from quieter voices heard
// while listening. Comparing the two (log-likelihood ratio) tells whether a loud
// voice is the wearer or a visitor. Everything runs on the phone in ~0.1 ms/frame.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

#include "Biquad.h"

namespace sunai {

class VoiceModel {
public:
    static constexpr int kDims = 9;   // log2 pitch + 8 band shapes

    void setBroadPrior() {
        // Covers all adult and child voices: pitch around 180 Hz +- 1 octave,
        // any spectral shape. Used for "other people" until they are heard.
        mMean[0] = std::log2(180.0f); mVar[0] = 0.6f;
        for (int i = 1; i < kDims; ++i) { mMean[i] = 0.0f; mVar[i] = 2.0f; }
    }
    void set(const float *mean, const float *var) {
        for (int i = 0; i < kDims; ++i) { mMean[i] = mean[i]; mVar[i] = std::max(var[i], minVar(i)); }
    }
    const float *mean() const { return mMean; }
    const float *var() const { return mVar; }

    float logLikelihood(const float *x) const {
        float ll = 0.0f;
        for (int i = 0; i < kDims; ++i) {
            float d = x[i] - mMean[i];
            ll -= 0.5f * (d * d / mVar[i] + std::log(mVar[i]));
        }
        return ll;
    }
    // Slow online update (exponential moving mean and variance).
    void adapt(const float *x, float rate) {
        for (int i = 0; i < kDims; ++i) {
            float d = x[i] - mMean[i];
            mMean[i] += rate * d;
            mVar[i] = std::max(minVar(i), (1.0f - rate) * mVar[i] + rate * d * d);
        }
    }
    static float minVar(int i) { return i == 0 ? 0.02f : 0.15f; }

private:
    float mMean[kDims] = {};
    float mVar[kDims] = {};
};

class VoiceFeatures {
public:
    static constexpr int kBands = 8;

    void setup(float fs) {
        mDecim = std::max(1, static_cast<int>(std::lround(fs / 12000.0f)));
        mFsd = fs / mDecim;
        mHop = static_cast<int>(std::lround(mFsd * 0.010f));   // 10 ms
        static const float centres[kBands] = {300, 500, 800, 1200, 1700, 2400, 3300, 4500};
        for (int b = 0; b < kBands; ++b) {
            float fc = std::min(centres[b], 0.45f * mFsd);
            mBank[b].setBandPass(mFsd, fc, 1.4f);
            mBank[b].reset();
        }
        mMinLag = static_cast<int>(mFsd / 400.0f);
        mMaxLag = std::min(kHist - kWin - 1, static_cast<int>(mFsd / 70.0f));
        reset();
    }

    void reset() {
        std::memset(mHist, 0, sizeof(mHist));
        std::memset(mBandAcc, 0, sizeof(mBandAcc));
        mHistPos = mDecimCount = mHopCount = 0;
        mDecimAcc = mEnergyAcc = 0.0f;
        mFrameReady = false;
    }

    void setPitchEnabled(bool on) { mPitchOn = on; }

    // Feed one sample (full rate). Returns true when a new 10 ms frame is ready.
    inline bool push(float x) {
        mEnergyAcc += x * x;
        mDecimAcc += x;
        if (++mDecimCount < mDecim) return false;
        float d = mDecimAcc / mDecim;
        mDecimCount = 0;
        mDecimAcc = 0.0f;

        mHist[mHistPos] = d;
        mHistPos = (mHistPos + 1) % kHist;
        for (int b = 0; b < kBands; ++b) {
            float y = mBank[b].process(d);
            mBandAcc[b] += y * y;
        }
        if (++mHopCount < mHop) return false;
        finishFrame();
        mHopCount = 0;
        return true;
    }

    // Results of the last frame.
    float levelDb() const { return mLevelDb; }    // loudness of the frame (dBFS)
    bool voiced() const { return mVoiced; }       // a clear pitched voice was found
    float pitchHz() const { return mPitchHz; }
    const float *features() const { return mFeat; }

private:
    void finishFrame() {
        const int fullSamples = mHop * mDecim;
        mLevelDb = 10.0f * std::log10(mEnergyAcc / fullSamples + 1e-12f);
        mEnergyAcc = 0.0f;

        // Spectral shape: band levels in units of 10 dB, minus their average.
        float sum = 0.0f;
        float lb[kBands];
        for (int b = 0; b < kBands; ++b) {
            lb[b] = std::log10(mBandAcc[b] / mHop + 1e-12f);
            sum += lb[b];
            mBandAcc[b] = 0.0f;
        }
        for (int b = 0; b < kBands; ++b) mFeat[1 + b] = lb[b] - sum / kBands;

        mVoiced = false;
        if (!mPitchOn || mLevelDb < -65.0f) return;

        // Normalised autocorrelation over the last kWin decimated samples.
        float buf[kWin + kHist];
        int n = kWin + mMaxLag;
        int start = (mHistPos - n + kHist * 2) % kHist;
        for (int i = 0; i < n; ++i) buf[i] = mHist[(start + i) % kHist];
        const float *cur = buf + mMaxLag;   // newest kWin samples
        float e0 = 0.0f;
        for (int i = 0; i < kWin; ++i) e0 += cur[i] * cur[i];
        if (e0 <= 1e-9f) return;
        float best = 0.0f;
        int bestLag = 0;
        for (int lag = mMinLag; lag <= mMaxLag; ++lag) {
            const float *past = cur - lag;
            float r = 0.0f, e1 = 0.0f;
            for (int i = 0; i < kWin; ++i) { r += cur[i] * past[i]; e1 += past[i] * past[i]; }
            float c = r / std::sqrt(e0 * e1 + 1e-12f);
            if (c > best) { best = c; bestLag = lag; }
        }
        if (best > 0.6f && bestLag > 0) {
            // Prefer the shortest lag that is nearly as good (avoids octave errors).
            for (int div = 4; div >= 2; --div) {
                int l = bestLag / div;
                if (l < mMinLag) continue;
                const float *past = cur - l;
                float r = 0.0f, e1 = 0.0f;
                for (int i = 0; i < kWin; ++i) { r += cur[i] * past[i]; e1 += past[i] * past[i]; }
                if (r / std::sqrt(e0 * e1 + 1e-12f) > 0.9f * best) { bestLag = l; break; }
            }
            mVoiced = true;
            mPitchHz = mFsd / bestLag;
            mFeat[0] = std::log2(mPitchHz);
        }
    }

    static constexpr int kWin = 300;    // ~25 ms at 12 kHz
    static constexpr int kHist = 512;   // enough for the window + the longest lag

    int mDecim = 4, mHop = 120, mMinLag = 30, mMaxLag = 171;
    float mFsd = 12000.0f;
    Biquad mBank[kBands];
    float mHist[kHist] = {};
    float mBandAcc[kBands] = {};
    int mHistPos = 0, mDecimCount = 0, mHopCount = 0;
    float mDecimAcc = 0.0f, mEnergyAcc = 0.0f;
    bool mPitchOn = true, mFrameReady = false;

    float mLevelDb = -120.0f, mPitchHz = 0.0f;
    bool mVoiced = false;
    float mFeat[VoiceModel::kDims] = {};
};

// Decides, frame by frame, whether a voice belongs to the wearer.
class SpeakerCheck {
public:
    SpeakerCheck() { mOther.setBroadPrior(); }

    bool hasProfile() const { return mHasProfile; }

    void setProfile(const float *mean, const float *var) {
        mWearer.set(mean, var);
        mHasProfile = true;
        mOther.setBroadPrior();
        mScore = 0.0f;
    }
    void clearProfile() { mHasProfile = false; mScore = 0.0f; }

    // Learning the wearer's voice (called for each frame while learning).
    void beginLearning() { mLearnN = 0; std::memset(mSum, 0, sizeof(mSum)); std::memset(mSum2, 0, sizeof(mSum2)); }
    void learn(const VoiceFeatures &f) {
        if (!f.voiced() || f.levelDb() < -55.0f) return;
        const float *x = f.features();
        for (int i = 0; i < VoiceModel::kDims; ++i) { mSum[i] += x[i]; mSum2[i] += x[i] * x[i]; }
        ++mLearnN;
    }
    int learnedFrames() const { return mLearnN; }
    // Finishes learning; returns false if too little voiced speech was heard.
    bool finishLearning(float *meanOut, float *varOut) {
        if (mLearnN < 40) return false;   // at least ~0.4 s of clear voice
        for (int i = 0; i < VoiceModel::kDims; ++i) {
            float m = mSum[i] / mLearnN;
            meanOut[i] = m;
            varOut[i] = std::max(VoiceModel::minVar(i), mSum2[i] / mLearnN - m * m);
        }
        setProfile(meanOut, varOut);
        return true;
    }

    // Normal listening. `loud` = above the own-voice level.
    void update(const VoiceFeatures &f, bool loud) {
        if (!mHasProfile) return;
        if (!f.voiced()) {
            mScore *= 0.9f;   // forget slowly in pauses, so a new loud onset counts as "wearer"
            return;
        }
        const float *x = f.features();
        float llr = mWearer.logLikelihood(x) - mOther.logLikelihood(x);
        llr = std::clamp(llr, -12.0f, 12.0f);
        mScore = 0.7f * mScore + 0.3f * llr;
        // Quieter voices are other people: learn what they sound like.
        if (!loud && f.levelDb() > -62.0f && llr < 2.0f) mOther.adapt(x, 0.01f);
    }

    // A loud voice that clearly does not match the wearer.
    bool otherSpeaker() const { return mHasProfile && mScore < -2.0f; }
    float score() const { return mScore; }

private:
    VoiceModel mWearer, mOther;
    bool mHasProfile = false;
    float mScore = 0.0f;
    float mSum[VoiceModel::kDims] = {}, mSum2[VoiceModel::kDims] = {};
    int mLearnN = 0;
};

}  // namespace sunai
