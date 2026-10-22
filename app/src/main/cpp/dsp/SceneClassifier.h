// Automatic place selection (Home / Talk / Crowd).
//
// Every 10 ms it receives the frame loudness and the AI's speech probability
// (from RNNoise's neural voice-activity output, when it is running). Every
// 3 seconds it looks at the background noise floor (the quiet moments between
// sounds) and how much of the time someone is talking, and picks a place.
// It only switches after two windows agree, so it does not flip back and forth.
#pragma once

#include <algorithm>
#include <atomic>

namespace sunai {

class SceneClassifier {
public:
    static constexpr int kWindow = 300;   // 3 s of 10 ms frames

    void reset() { mCount = 0; mPending = -1; mPendingVotes = 0; }

    // speechProb < 0 means "unknown" (AI not running).
    void push(float levelDb, float speechProb) {
        mLevels[mCount] = levelDb;
        if (speechProb >= 0.0f) { ++mProbFrames; if (speechProb > 0.6f) ++mSpeechFrames; }
        if (++mCount < kWindow) return;
        decide();
        mCount = 0;
        mProbFrames = mSpeechFrames = 0;
    }

    int place() const { return mPlace.load(); }     // 0 home, 1 talk, 2 crowd
    float noiseFloorDb() const { return mFloorDb.load(); }
    float speechRatio() const { return mSpeechRatio.load(); }

private:
    void decide() {
        // Noise floor = the 10th-percentile frame level (the gaps between sounds).
        float sorted[kWindow];
        std::copy(mLevels, mLevels + kWindow, sorted);
        std::nth_element(sorted, sorted + kWindow / 10, sorted + kWindow);
        float floorDb = sorted[kWindow / 10];
        float speech = mProbFrames > kWindow / 2 ? float(mSpeechFrames) / mProbFrames : -1.0f;
        mFloorDb.store(floorDb);
        mSpeechRatio.store(speech);

        int guess;
        if (floorDb > -42.0f) guess = 2;                          // loud background: market, bus, road
        else if (floorDb > -54.0f) guess = 1;                     // some background noise
        else if (speech >= 0.0f && speech > 0.5f && floorDb > -60.0f) guess = 1;  // quiet but busy talk
        else guess = 0;                                           // quiet room

        if (guess == mPlace.load()) { mPending = -1; mPendingVotes = 0; return; }
        if (guess == mPending) {
            if (++mPendingVotes >= 2) { mPlace.store(guess); mPending = -1; mPendingVotes = 0; }
        } else {
            mPending = guess;
            mPendingVotes = 1;
        }
    }

    float mLevels[kWindow] = {};
    int mCount = 0, mProbFrames = 0, mSpeechFrames = 0;
    int mPending = -1, mPendingVotes = 0;
    std::atomic<int> mPlace{1};
    std::atomic<float> mFloorDb{-120.0f}, mSpeechRatio{-1.0f};
};

}  // namespace sunai
