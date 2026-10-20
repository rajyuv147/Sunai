// AI noise reduction using RNNoise (a ~100 KB recurrent neural network).
// RNNoise works on 10 ms frames at 48 kHz, so when it is on it adds exactly
// 480 samples (10 ms) of delay. When it is off it is fully bypassed and costs
// nothing. Switching fades over one frame to avoid clicks.
#pragma once

#include <algorithm>
#include <cstring>

extern "C" {
#include "rnnoise.h"
}

namespace sunai {

class NoiseReducer {
public:
    static constexpr int kFrame = 480;

    NoiseReducer() { mState = rnnoise_create(nullptr); }
    ~NoiseReducer() { if (mState) rnnoise_destroy(mState); }
    NoiseReducer(const NoiseReducer &) = delete;
    NoiseReducer &operator=(const NoiseReducer &) = delete;

    // Only usable at 48 kHz (the rate the network was trained for).
    void setSampleRate(float fs) { mUsable = (fs > 47999.0f && fs < 48001.0f); }
    bool usable() const { return mUsable && mState != nullptr; }

    void setEnabled(bool on) { mTarget = (on && usable()) ? 1.0f : 0.0f; }
    bool active() const { return mMix > 0.0f || mTarget > 0.0f; }
    int latencyFrames() const { return mTarget > 0.0f ? kFrame : 0; }

    inline float process(float x) {
        if (!active()) return x;
        mIn[mPos] = x * 32768.0f;
        float wet = mOut[mPos] * (1.0f / 32768.0f);
        if (++mPos == kFrame) {
            mPos = 0;
            rnnoise_process_frame(mState, mOut, mIn);
        }
        // Ramp the mix toward the target over one frame.
        const float step = 1.0f / kFrame;
        if (mMix < mTarget) mMix = std::min(mTarget, mMix + step);
        else if (mMix > mTarget) mMix = std::max(mTarget, mMix - step);
        if (mMix == 0.0f && mTarget == 0.0f) {
            // Fully off again: clear buffers so the next switch-on starts clean.
            std::memset(mIn, 0, sizeof(mIn));
            std::memset(mOut, 0, sizeof(mOut));
            mPos = 0;
        }
        return mMix * wet + (1.0f - mMix) * x;
    }

private:
    DenoiseState *mState = nullptr;
    bool mUsable = false;
    float mIn[kFrame] = {};
    float mOut[kFrame] = {};
    int mPos = 0;
    float mMix = 0.0f, mTarget = 0.0f;
};

}  // namespace sunai
