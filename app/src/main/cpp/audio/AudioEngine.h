// Live microphone -> earphone engine on Oboe.
//
// The microphone and the earphone run as two independent low-latency streams.
// The mic callback pushes samples into a small lock-free ring buffer; the
// earphone callback pulls from it, processes and plays. If the mic stalls the
// earphone keeps running (silence) instead of stopping, and the app can detect
// the stall and reopen with safer settings ("fallback levels").
#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <oboe/Oboe.h>

#include "../dsp/Processor.h"

namespace sunai {

class AudioEngine : public oboe::AudioStreamDataCallback,
                    public oboe::AudioStreamErrorCallback {
public:
    // 0 = fastest (exclusive streams, live-performance mic preset, chosen devices)
    // 1 = shared streams
    // 2 = shared + speech-recognition mic preset
    // 3 = safest: Android chooses mic and earphone, default preset
    static constexpr int kMaxFallback = 3;

    AudioEngine() = default;
    ~AudioEngine() override { stopEngine(); }

    // Device ids come from Android's AudioDeviceInfo.getId(); 0 = let Android choose.
    bool startEngine(int outputDeviceId, int inputDeviceId, int fallbackLevel);
    void stopEngine();
    bool running() const { return mRunning.load(); }

    Processor &processor() { return mProcessor; }

    float latencyMs();
    bool fastPath() const { return mFastPath.load(); }
    int sampleRate() const { return mSampleRate.load(); }
    int fallbackLevel() const { return mLevel.load(); }

    // True when the earphone has been playing for over a second with no mic data.
    bool micStalled() const;
    // One line of numbers for the helper screen and bug reports.
    std::string diagnostics();

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *stream, void *data, int32_t frames) override;
    void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result error) override;

private:
    bool openLocked();
    void closeLocked();
    void restartAsync();
    void onInput(const float *data, int32_t frames, int32_t channels);
    void onOutput(float *data, int32_t frames);
    void setError(const std::string &e);

    std::mutex mLock;
    std::shared_ptr<oboe::AudioStream> mIn, mOut;
    // Grows the earphone buffer a little if the phone cannot keep up (prevents crackling).
    std::unique_ptr<oboe::LatencyTuner> mTuner;
    std::atomic<float> mCpuLoad{0.0f};   // share of each audio period spent processing
    Processor mProcessor;

    // Ring buffer (single producer: mic thread, single consumer: earphone thread).
    static constexpr uint32_t kRingSize = 8192;   // power of two, ~170 ms at 48 kHz
    float mRing[kRingSize] = {};
    std::atomic<uint32_t> mWrite{0}, mRead{0};
    bool mPrimed = false;
    std::vector<float> mInScratch, mOutScratch;
    int mOutChannels = 2;
    int mInBurst = 192, mOutBurst = 192;

    int mOutputDeviceId = 0, mInputDeviceId = 0;
    std::atomic<int> mLevel{0};
    std::atomic<bool> mWantRunning{false}, mRunning{false}, mFastPath{false};
    std::atomic<int> mSampleRate{0};

    // Diagnostics
    std::atomic<uint64_t> mInFrames{0}, mOutFrames{0}, mUnderruns{0}, mDropped{0};
    std::atomic<float> mMicPeak{0.0f};
    std::atomic<bool> mMicHeard{false};   // any non-zero mic sample since start
    float mMicPeakAcc = 0.0f;
    int mMicPeakCount = 0;
    std::mutex mErrLock;
    std::string mLastError = "-";
    std::string mConfig = "-";
};

}  // namespace sunai
