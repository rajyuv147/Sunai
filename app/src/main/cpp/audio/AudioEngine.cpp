#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

#ifdef __ANDROID__
#include <android/api-level.h>
#include <android/log.h>
#define LOG_TAG "SunaiAudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#define LOGW(...) ((void)0)
static int android_get_device_api_level() { return 30; }
#endif

namespace sunai {

bool AudioEngine::startEngine(int outputDeviceId, int inputDeviceId, int fallbackLevel) {
    std::lock_guard<std::mutex> guard(mLock);
    mOutputDeviceId = outputDeviceId;
    mInputDeviceId = inputDeviceId;
    mLevel.store(std::clamp(fallbackLevel, 0, kMaxFallback));
    mWantRunning.store(true);
    closeLocked();
    // If this level cannot open at all, step down automatically.
    while (!openLocked()) {
        if (mLevel.load() >= kMaxFallback) return false;
        mLevel.store(mLevel.load() + 1);
    }
    return true;
}

void AudioEngine::stopEngine() {
    std::lock_guard<std::mutex> guard(mLock);
    mWantRunning.store(false);
    closeLocked();
}

bool AudioEngine::openLocked() {
    const int level = mLevel.load();
    const bool exclusive = level == 0;
    const bool chooseDevices = level < 3;
    const auto sharing = exclusive ? oboe::SharingMode::Exclusive : oboe::SharingMode::Shared;

    // ---- Earphone (output) first: its native rate decides the rate for both. ----
    oboe::AudioStreamBuilder out;
    out.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(sharing)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setUsage(oboe::Usage::Media)
        ->setContentType(oboe::ContentType::Speech)
        ->setFormatConversionAllowed(true)
        ->setChannelConversionAllowed(true)
        ->setDataCallback(this)
        ->setErrorCallback(this);
    if (chooseDevices && mOutputDeviceId > 0) out.setDeviceId(mOutputDeviceId);

    oboe::Result r = out.openStream(mOut);
    if (r != oboe::Result::OK) {
        setError(std::string("out open: ") + oboe::convertToText(r));
        LOGW("L%d open failed", level);
        mOut.reset();
        return false;
    }
    const int rate = mOut->getSampleRate();
    mOutChannels = mOut->getChannelCount();
    mOutBurst = std::max(1, mOut->getFramesPerBurst());
    // Two bursts is the smallest buffer that stays glitch-free on most phones.
    mOut->setBufferSizeInFrames(std::min(mOut->getBufferCapacityInFrames(), mOutBurst * 2));

    // ---- Phone microphone (input) at the same rate. ----
    oboe::InputPreset preset = oboe::InputPreset::VoiceRecognition;
    if (level <= 1 && android_get_device_api_level() >= 29) preset = oboe::InputPreset::VoicePerformance;
    if (level == 3) preset = oboe::InputPreset::Generic;

    oboe::AudioStreamBuilder in;
    in.setDirection(oboe::Direction::Input)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(sharing)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Mono)
        ->setSampleRate(rate)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Fastest)
        ->setFormatConversionAllowed(true)
        ->setChannelConversionAllowed(true)
        ->setInputPreset(preset)
        ->setDataCallback(this)
        ->setErrorCallback(this);
    if (chooseDevices && mInputDeviceId > 0) in.setDeviceId(mInputDeviceId);

    r = in.openStream(mIn);
    if (r != oboe::Result::OK) {
        setError(std::string("in open: ") + oboe::convertToText(r));
        LOGW("L%d open failed", level);
        mIn.reset();
        mOut->close();
        mOut.reset();
        return false;
    }
    mInBurst = std::max(1, mIn->getFramesPerBurst());

    // Reset buffers and counters.
    mWrite.store(0);
    mRead.store(0);
    mPrimed = false;
    size_t maxFrames = static_cast<size_t>(std::max(mOut->getBufferCapacityInFrames(), 4096));
    mInScratch.assign(maxFrames, 0.0f);
    mOutScratch.assign(maxFrames * 2, 0.0f);
    mInFrames.store(0);
    mOutFrames.store(0);
    mUnderruns.store(0);
    mDropped.store(0);
    mMicPeak.store(0.0f);
    mMicHeard.store(false);

    mSampleRate.store(rate);
    mProcessor.prepare(static_cast<float>(rate));
    mFastPath.store(mOut->getPerformanceMode() == oboe::PerformanceMode::LowLatency &&
                    mIn->getPerformanceMode() == oboe::PerformanceMode::LowLatency);

    // Start the mic first so data is waiting when the earphone asks for it.
    r = mIn->requestStart();
    if (r == oboe::Result::OK) r = mOut->requestStart();
    if (r != oboe::Result::OK) {
        setError(std::string("start: ") + oboe::convertToText(r));
        LOGW("L%d open failed", level);
        closeLocked();
        return false;
    }

    char cfg[160];
    std::snprintf(cfg, sizeof(cfg), "L%d %dHz out:%s/%s b%d in:%s/%s b%d", level, rate,
                  mOut->getSharingMode() == oboe::SharingMode::Exclusive ? "ex" : "sh",
                  mOut->getPerformanceMode() == oboe::PerformanceMode::LowLatency ? "fast" : "norm",
                  mOutBurst,
                  mIn->getSharingMode() == oboe::SharingMode::Exclusive ? "ex" : "sh",
                  mIn->getPerformanceMode() == oboe::PerformanceMode::LowLatency ? "fast" : "norm",
                  mInBurst);
    mConfig = cfg;
    setError("-");
    mRunning.store(true);
    LOGI("running: %s", mConfig.c_str());
    return true;
}

void AudioEngine::closeLocked() {
    mRunning.store(false);
    if (mOut) { mOut->requestStop(); mOut->close(); mOut.reset(); }
    if (mIn) { mIn->requestStop(); mIn->close(); mIn.reset(); }
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream *stream, void *data, int32_t frames) {
    if (stream->getDirection() == oboe::Direction::Input) {
        onInput(static_cast<const float *>(data), frames, stream->getChannelCount());
    } else {
        onOutput(static_cast<float *>(data), frames);
    }
    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::onInput(const float *data, int32_t frames, int32_t channels) {
    uint32_t w = mWrite.load(std::memory_order_relaxed);
    uint32_t r = mRead.load(std::memory_order_acquire);
    uint32_t space = kRingSize - (w - r);
    int32_t n = std::min<int32_t>(frames, static_cast<int32_t>(space));
    float peak = mMicPeakAcc;
    for (int32_t i = 0; i < n; ++i) {
        float x = data[i * channels];
        mRing[(w + i) & (kRingSize - 1)] = x;
        peak = std::max(peak, std::fabs(x));
    }
    if (peak > 1e-6f) mMicHeard.store(true);
    mWrite.store(w + n, std::memory_order_release);
    if (n < frames) mDropped.fetch_add(frames - n);
    mInFrames.fetch_add(frames);

    mMicPeakAcc = peak;
    mMicPeakCount += frames;
    if (mMicPeakCount >= mSampleRate.load() / 2) {   // publish every ~0.5 s
        mMicPeak.store(mMicPeakAcc);
        mMicPeakAcc = 0.0f;
        mMicPeakCount = 0;
    }
}

void AudioEngine::onOutput(float *out, int32_t frames) {
    uint32_t r = mRead.load(std::memory_order_relaxed);
    uint32_t w = mWrite.load(std::memory_order_acquire);
    uint32_t avail = w - r;

    // Keep the buffer short so the delay stays low: drop anything beyond a small cushion.
    uint32_t maxKeep = static_cast<uint32_t>(2 * mInBurst + 2 * frames);
    if (avail > maxKeep) {
        mDropped.fetch_add(avail - maxKeep);
        r += avail - maxKeep;
        avail = maxKeep;
    }
    // Wait for a small cushion before playing (and again after running dry).
    uint32_t primeAt = static_cast<uint32_t>(frames + mInBurst);
    if (!mPrimed && avail >= primeAt) mPrimed = true;

    int32_t take = 0;
    if (mPrimed) {
        take = std::min<int32_t>(frames, static_cast<int32_t>(avail));
        if (take < frames) { mPrimed = false; mUnderruns.fetch_add(1); }
        take = std::min<int32_t>(take, static_cast<int32_t>(mInScratch.size()));
        for (int32_t i = 0; i < take; ++i) mInScratch[i] = mRing[(r + i) & (kRingSize - 1)];
        r += take;
    }
    mRead.store(r, std::memory_order_release);

    if (mOutChannels == 2) {
        mProcessor.process(mInScratch.data(), take, out, frames);
    } else {
        int32_t n = std::min<int32_t>(frames, static_cast<int32_t>(mOutScratch.size() / 2));
        mProcessor.process(mInScratch.data(), take, mOutScratch.data(), n);
        for (int32_t i = 0; i < n; ++i) {
            float m = 0.5f * (mOutScratch[2 * i] + mOutScratch[2 * i + 1]);
            for (int c = 0; c < mOutChannels; ++c) out[i * mOutChannels + c] = m;
        }
    }
    mOutFrames.fetch_add(frames);
}

void AudioEngine::onErrorAfterClose(oboe::AudioStream *stream, oboe::Result error) {
    setError(std::string(stream->getDirection() == oboe::Direction::Input ? "in " : "out ") +
                 oboe::convertToText(error));
    LOGW("stream closed");
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
        while (!openLocked()) {
            if (mLevel.load() >= kMaxFallback) return;
            mLevel.store(mLevel.load() + 1);
        }
    }).detach();
}

void AudioEngine::setError(const std::string &e) {
    std::lock_guard<std::mutex> g(mErrLock);
    mLastError = e;
}

bool AudioEngine::micStalled() const {
    if (!mRunning.load()) return false;
    int rate = mSampleRate.load();
    if (rate <= 0) return false;
    uint64_t in = mInFrames.load();
    if (in == 0) return mOutFrames.load() > static_cast<uint64_t>(rate);
    return in > static_cast<uint64_t>(2 * rate) && !mMicHeard.load();
}

float AudioEngine::latencyMs() {
    std::lock_guard<std::mutex> guard(mLock);
    if (!mRunning.load() || !mOut || !mIn) return -1.0f;
    auto outLat = mOut->calculateLatencyMillis();
    auto inLat = mIn->calculateLatencyMillis();
    if (!outLat) return -1.0f;
    double total = outLat.value() + (inLat ? inLat.value() : 0.0);
    int rate = mSampleRate.load();
    if (rate > 0) {
        uint32_t fill = mWrite.load() - mRead.load();
        total += 1000.0 * (mProcessor.extraLatencyFrames() + fill) / rate;
    }
    return static_cast<float>(total);
}

std::string AudioEngine::diagnostics() {
    std::lock_guard<std::mutex> guard(mLock);
    float peak = mMicPeak.load();
    float peakDb = peak > 0.0f ? 20.0f * std::log10(peak) : -120.0f;
    std::string err;
    { std::lock_guard<std::mutex> g(mErrLock); err = mLastError; }
    char buf[320];
#ifdef SUNAI_RNNOISE_V2
    const char *model = "rnn0.2";
#else
    const char *model = "rnn0.1";
#endif
    std::snprintf(buf, sizeof(buf), "%s %s | %s | mic %llu (%.0f dB) out %llu | under %llu drop %llu | err %s",
                  mRunning.load() ? "ON" : "OFF", model, mConfig.c_str(),
                  static_cast<unsigned long long>(mInFrames.load()), peakDb,
                  static_cast<unsigned long long>(mOutFrames.load()),
                  static_cast<unsigned long long>(mUnderruns.load()),
                  static_cast<unsigned long long>(mDropped.load()), err.c_str());
    return buf;
}

}  // namespace sunai
