#include "AudioEngine.h"

#include <algorithm>
#include <android/api-level.h>
#include <android/log.h>
#include <thread>

#define LOG_TAG "SunaiAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace sunai {

bool AudioEngine::startEngine(int outputDeviceId, int inputDeviceId) {
    std::lock_guard<std::mutex> guard(mLock);
    mOutputDeviceId = outputDeviceId;
    mInputDeviceId = inputDeviceId;
    mWantRunning.store(true);
    if (mRunning.load()) closeLocked();
    return openLocked();
}

void AudioEngine::stopEngine() {
    std::lock_guard<std::mutex> guard(mLock);
    mWantRunning.store(false);
    closeLocked();
}

bool AudioEngine::openLocked() {
    // ---- Output (earphone) first: its native rate decides the rate for both. ----
    oboe::AudioStreamBuilder out;
    out.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setUsage(oboe::Usage::Media)
        ->setContentType(oboe::ContentType::Speech)
        ->setFormatConversionAllowed(true)
        ->setChannelConversionAllowed(true)
        ->setDataCallback(this)
        ->setErrorCallback(this);
    if (mOutputDeviceId > 0) out.setDeviceId(mOutputDeviceId);

    oboe::Result r = out.openStream(mOut);
    if (r != oboe::Result::OK) {
        LOGW("output open failed: %s", oboe::convertToText(r));
        mOut.reset();
        return false;
    }
    // Two bursts is the smallest buffer that stays glitch-free on most phones.
    mOut->setBufferSizeInFrames(mOut->getFramesPerBurst() * 2);
    const int rate = mOut->getSampleRate();
    mOutChannels = mOut->getChannelCount();

    // ---- Input (phone microphone) at the same rate, read by the full-duplex helper. ----
    oboe::AudioStreamBuilder in;
    in.setDirection(oboe::Direction::Input)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Mono)
        ->setSampleRate(rate)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Fastest)
        ->setFormatConversionAllowed(true)
        ->setChannelConversionAllowed(true)
        // "VoicePerformance" is Android's preset for live, low-latency processing
        // (no echo cancellation or auto-gain that would fight our own processing).
        ->setInputPreset(android_get_device_api_level() >= 29
                             ? oboe::InputPreset::VoicePerformance
                             : oboe::InputPreset::VoiceRecognition);
    if (mInputDeviceId > 0) in.setDeviceId(mInputDeviceId);

    r = in.openStream(mIn);
    if (r != oboe::Result::OK) {
        LOGW("input open failed: %s", oboe::convertToText(r));
        mIn.reset();
        mOut->close();
        mOut.reset();
        return false;
    }

    mSampleRate.store(rate);
    mProcessor.prepare(static_cast<float>(rate));
    mStereoScratch.assign(static_cast<size_t>(mOut->getBufferCapacityInFrames()) * 2, 0.0f);
    mFastPath.store(mOut->getPerformanceMode() == oboe::PerformanceMode::LowLatency &&
                    mIn->getPerformanceMode() == oboe::PerformanceMode::LowLatency);

    setSharedInputStream(mIn);
    setSharedOutputStream(mOut);
    r = FullDuplexStream::start();
    if (r != oboe::Result::OK) {
        LOGW("start failed: %s", oboe::convertToText(r));
        closeLocked();
        return false;
    }
    mRunning.store(true);
    LOGI("running: %d Hz, burst %d, fast path %d, out ch %d", rate, mOut->getFramesPerBurst(),
         mFastPath.load() ? 1 : 0, mOutChannels);
    return true;
}

void AudioEngine::closeLocked() {
    mRunning.store(false);
    // Stop each stream directly (the helper's stop() only stops the output).
    if (mOut) { mOut->requestStop(); mOut->close(); mOut.reset(); }
    if (mIn) { mIn->requestStop(); mIn->close(); mIn.reset(); }
}

oboe::DataCallbackResult AudioEngine::onBothStreamsReady(const void *inputData, int numInputFrames,
                                                         void *outputData, int numOutputFrames) {
    const float *in = static_cast<const float *>(inputData);
    float *out = static_cast<float *>(outputData);
    if (mOutChannels == 2) {
        mProcessor.process(in, numInputFrames, out, numOutputFrames);
    } else {
        // Mono earphone: process in stereo, then fold down.
        int frames = std::min(numOutputFrames, static_cast<int>(mStereoScratch.size() / 2));
        mProcessor.process(in, numInputFrames, mStereoScratch.data(), frames);
        for (int i = 0; i < frames; ++i) {
            out[i * mOutChannels] = 0.5f * (mStereoScratch[2 * i] + mStereoScratch[2 * i + 1]);
            for (int c = 1; c < mOutChannels; ++c) out[i * mOutChannels + c] = out[i * mOutChannels];
        }
    }
    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::onErrorAfterClose(oboe::AudioStream * /*stream*/, oboe::Result error) {
    LOGW("stream closed: %s", oboe::convertToText(error));
    mRunning.store(false);
    // Earphone unplugged or Bluetooth dropped: reopen on the new default device.
    if (mWantRunning.load()) restartAsync();
}

void AudioEngine::restartAsync() {
    // Oboe must not be restarted from its own callback thread.
    std::thread([this]() {
        std::lock_guard<std::mutex> guard(mLock);
        if (!mWantRunning.load()) return;
        closeLocked();
        mOutputDeviceId = 0;   // the old device may be gone
        openLocked();
    }).detach();
}

float AudioEngine::latencyMs() {
    std::lock_guard<std::mutex> guard(mLock);
    if (!mRunning.load() || !mOut || !mIn) return -1.0f;
    auto outLat = mOut->calculateLatencyMillis();
    auto inLat = mIn->calculateLatencyMillis();
    if (!outLat) return -1.0f;
    double total = outLat.value();
    total += inLat ? inLat.value() : 0.0;
    int rate = mSampleRate.load();
    if (rate > 0) total += 1000.0 * mProcessor.extraLatencyFrames() / rate;
    return static_cast<float>(total);
}

}  // namespace sunai
