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

JNIEXPORT jboolean JNICALL FN(start)(JNIEnv *, jobject, jint outputDeviceId, jint inputDeviceId,
                                     jint fallbackLevel) {
    return engine().startEngine(outputDeviceId, inputDeviceId, fallbackLevel) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jboolean JNICALL FN(isMicStalled)(JNIEnv *, jobject) { return engine().micStalled(); }
JNIEXPORT jint JNICALL FN(getFallbackLevel)(JNIEnv *, jobject) { return engine().fallbackLevel(); }
JNIEXPORT jstring JNICALL FN(getDiagnostics)(JNIEnv *env, jobject) {
    return env->NewStringUTF(engine().diagnostics().c_str());
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
JNIEXPORT void JNICALL FN(setOwnVoice)(JNIEnv *, jobject, jboolean on, jfloat thresholdDb) {
    engine().processor().setOwnVoice(on, thresholdDb);
}
JNIEXPORT jfloat JNICALL FN(getInputLevelDb)(JNIEnv *, jobject) { return engine().processor().inputLevelDb(); }
JNIEXPORT jboolean JNICALL FN(isOwnVoiceActive)(JNIEnv *, jobject) { return engine().processor().ownVoiceActive(); }
JNIEXPORT void JNICALL FN(setAutoPlace)(JNIEnv *, jobject, jboolean on) { engine().processor().setAutoPlace(on); }
JNIEXPORT jint JNICALL FN(getAutoPlace)(JNIEnv *, jobject) { return engine().processor().autoPlace(); }
JNIEXPORT jfloat JNICALL FN(getNoiseFloorDb)(JNIEnv *, jobject) { return engine().processor().noiseFloorDb(); }
JNIEXPORT void JNICALL FN(setVoiceProfile)(JNIEnv *env, jobject, jfloatArray profile) {
    if (!profile || env->GetArrayLength(profile) != sunai::Processor::kProfile) {
        engine().processor().setVoiceProfile(nullptr);
        return;
    }
    float buf[sunai::Processor::kProfile];
    env->GetFloatArrayRegion(profile, 0, sunai::Processor::kProfile, buf);
    engine().processor().setVoiceProfile(buf);
}
JNIEXPORT void JNICALL FN(startVoiceLearning)(JNIEnv *, jobject) { engine().processor().startVoiceLearning(); }
JNIEXPORT void JNICALL FN(stopVoiceLearning)(JNIEnv *, jobject) { engine().processor().stopVoiceLearning(); }
JNIEXPORT jint JNICALL FN(getVoiceLearningState)(JNIEnv *, jobject) {
    return engine().processor().voiceLearningState(nullptr);
}
JNIEXPORT jfloatArray JNICALL FN(getLearnedVoiceProfile)(JNIEnv *env, jobject) {
    float buf[sunai::Processor::kProfile];
    if (engine().processor().voiceLearningState(buf) != 3) return nullptr;
    jfloatArray arr = env->NewFloatArray(sunai::Processor::kProfile);
    env->SetFloatArrayRegion(arr, 0, sunai::Processor::kProfile, buf);
    return arr;
}
JNIEXPORT jboolean JNICALL FN(hasVoiceProfile)(JNIEnv *, jobject) { return engine().processor().hasVoiceProfile(); }
JNIEXPORT jfloat JNICALL FN(getSpeakerScore)(JNIEnv *, jobject) { return engine().processor().speakerScore(); }
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
