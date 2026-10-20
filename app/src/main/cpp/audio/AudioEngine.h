// Live microphone -> earphone engine built on Oboe's full-duplex helper.
// Both streams ask Android for the low-latency path (AAudio, exclusive/MMAP
// where the phone supports it) and use the smallest stable buffer.
#pragma once

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include <oboe/Oboe.h>

#include "../dsp/Processor.h"

namespace sunai {

class AudioEngine : public oboe::FullDuplexStream,
                    public oboe::AudioStreamErrorCallback {
public:
    AudioEngine() = default;
    ~AudioEngine() override { stopEngine(); }

    // Device ids come from Android's AudioDeviceInfo.getId(); 0 = let Android choose.
    bool startEngine(int outputDeviceId, int inputDeviceId);
    void stopEngine();
    bool running() const { return mRunning.load(); }

    Processor &processor() { return mProcessor; }

    // Measured end-to-end delay estimate in ms (mic + processing + output), or -1.
    float latencyMs();
    bool fastPath() const { return mFastPath.load(); }
    int sampleRate() const { return mSampleRate.load(); }

    // oboe::FullDuplexStream
    oboe::DataCallbackResult onBothStreamsReady(const void *inputData, int numInputFrames,
                                                void *outputData, int numOutputFrames) override;
    // oboe::AudioStreamErrorCallback
    void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result error) override;

private:
    bool openLocked();
    void closeLocked();
    void restartAsync();

    std::mutex mLock;
    std::shared_ptr<oboe::AudioStream> mIn, mOut;
    Processor mProcessor;
    std::vector<float> mStereoScratch;   // used only if the output device is mono
    int mOutChannels = 2;

    int mOutputDeviceId = 0, mInputDeviceId = 0;
    std::atomic<bool> mWantRunning{false}, mRunning{false}, mFastPath{false};
    std::atomic<int> mSampleRate{0};
};

}  // namespace sunai
