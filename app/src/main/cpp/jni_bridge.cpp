// JNI functions for np.sunai.audio.NativeAudio (a Kotlin `object`).
#include <jni.h>

#include "audio/AudioEngine.h"

using sunai::AudioEngine;

namespace {
AudioEngine &engine() {
    static AudioEngine *instance = new AudioEngine();   // lives for the whole app
    return *instance;
}
}  // namespace

#define FN(name) Java_np_sunai_audio_NativeAudio_##name

extern "C" {

JNIEXPORT jboolean JNICALL FN(start)(JNIEnv *, jobject, jint outputDeviceId, jint inputDeviceId) {
    return engine().startEngine(outputDeviceId, inputDeviceId) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL FN(stop)(JNIEnv *, jobject) { engine().stopEngine(); }
JNIEXPORT jboolean JNICALL FN(isRunning)(JNIEnv *, jobject) { return engine().running(); }

JNIEXPORT void JNICALL FN(setVolume)(JNIEnv *, jobject, jint v) { engine().processor().setVolume(v); }
JNIEXPORT void JNICALL FN(setPlace)(JNIEnv *, jobject, jint p) { engine().processor().setPlace(p); }
JNIEXPORT void JNICALL FN(setBoost)(JNIEnv *, jobject, jint ear, jint band, jfloat db) {
    engine().processor().setBoost(ear, band, db);
}
JNIEXPORT void JNICALL FN(setBalance)(JNIEnv *, jobject, jint b) { engine().processor().setBalance(b); }
JNIEXPORT void JNICALL FN(setSafety)(JNIEnv *, jobject, jboolean on) { engine().processor().setSafety(on); }
JNIEXPORT void JNICALL FN(setSaver)(JNIEnv *, jobject, jboolean on) { engine().processor().setSaver(on); }
JNIEXPORT void JNICALL FN(setLowDelay)(JNIEnv *, jobject, jboolean on) { engine().processor().setLowDelay(on); }
JNIEXPORT void JNICALL FN(setMuted)(JNIEnv *, jobject, jboolean on) { engine().processor().setMuted(on); }

JNIEXPORT void JNICALL FN(playTone)(JNIEnv *, jobject, jfloat freq, jint ear, jfloat levelDb, jfloat ms) {
    engine().processor().tones().request(freq, ear, levelDb, ms);
}
JNIEXPORT void JNICALL FN(stopTone)(JNIEnv *, jobject) { engine().processor().tones().cancel(); }
JNIEXPORT jboolean JNICALL FN(isTonePlaying)(JNIEnv *, jobject) { return engine().processor().tones().playing(); }

JNIEXPORT jfloat JNICALL FN(getLevel)(JNIEnv *, jobject) { return engine().processor().level(); }
JNIEXPORT jfloat JNICALL FN(getLatencyMs)(JNIEnv *, jobject) { return engine().latencyMs(); }
JNIEXPORT jboolean JNICALL FN(isFastPath)(JNIEnv *, jobject) { return engine().fastPath(); }
JNIEXPORT jint JNICALL FN(getSampleRate)(JNIEnv *, jobject) { return engine().sampleRate(); }
JNIEXPORT jboolean JNICALL FN(isHowling)(JNIEnv *, jobject) { return engine().processor().howlDetected(); }
JNIEXPORT jboolean JNICALL FN(isNoiseReductionActive)(JNIEnv *, jobject) {
    return engine().processor().noiseReductionActive();
}

}  // extern "C"
