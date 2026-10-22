// Pure tones for the ear test, played into one ear with soft fade in/out.
#pragma once

#include <atomic>
#include <cmath>

namespace sunai {

class ToneGenerator {
public:
    void setSampleRate(float fs) { mFs = fs; }

    // Called from the UI thread.
    void request(float freqHz, int ear, float levelDb, float durationMs) {
        mReqFreq.store(freqHz);
        mReqEar.store(ear);
        mReqAmp.store(std::pow(10.0f, levelDb / 20.0f));
        mReqLen.store(static_cast<int>(durationMs * 0.001f * mFs));
        mPending.store(true);
    }
    void cancel() { mCancel.store(true); }
    bool playing() const { return mPlaying.load(); }

    // Audio thread. Writes into L/R when playing; returns false when silent.
    inline bool next(float &left, float &right) {
        if (mCancel.exchange(false)) { mRemaining = 0; mPlaying.store(false); }
        if (mPending.exchange(false)) {
            mFreq = mReqFreq.load(); mEar = mReqEar.load(); mAmp = mReqAmp.load();
            mLength = mRemaining = mReqLen.load();
            mPhase = 0.0f;
            mPlaying.store(mLength > 0);
        }
        if (mRemaining <= 0) return false;
        int pos = mLength - mRemaining;
        int fade = static_cast<int>(0.03f * mFs);  // 30 ms raised-cosine fades
        float env = 1.0f;
        if (pos < fade) env = 0.5f - 0.5f * std::cos(kPi * pos / fade);
        else if (mRemaining < fade) env = 0.5f - 0.5f * std::cos(kPi * mRemaining / fade);
        float s = mAmp * env * std::sin(mPhase);
        mPhase += 2.0f * kPi * mFreq / mFs;
        if (mPhase > 2.0f * kPi) mPhase -= 2.0f * kPi;
        left = (mEar == 0 || mEar == 2) ? s : 0.0f;    // ear 2 = both ears
        right = (mEar == 1 || mEar == 2) ? s : 0.0f;
        if (--mRemaining == 0) mPlaying.store(false);
        return true;
    }

private:
    static constexpr float kPi = 3.14159265358979f;
    float mFs = 48000.0f;
    std::atomic<float> mReqFreq{1000.0f}, mReqAmp{0.0f};
    std::atomic<int> mReqEar{0}, mReqLen{0};
    std::atomic<bool> mPending{false}, mCancel{false}, mPlaying{false};
    float mFreq = 1000.0f, mAmp = 0.0f, mPhase = 0.0f;
    int mEar = 0, mLength = 0, mRemaining = 0;
};

}  // namespace sunai
