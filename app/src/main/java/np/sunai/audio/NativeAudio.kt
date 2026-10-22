package np.sunai.audio

/** Thin bridge to the C++ sound engine (see src/main/cpp/jni_bridge.cpp). */
object NativeAudio {
    init {
        System.loadLibrary("sunai")
    }

    /**
     * Device ids from AudioDeviceInfo.id; 0 lets Android choose.
     * fallbackLevel 0 = fastest settings … 3 = safest settings (see AudioEngine.h).
     */
    external fun start(outputDeviceId: Int, inputDeviceId: Int, fallbackLevel: Int): Boolean
    /** True when the earphone has played for over a second with no microphone data. */
    external fun isMicStalled(): Boolean
    external fun getFallbackLevel(): Int
    /** One line of engine numbers for the helper screen. */
    external fun getDiagnostics(): String
    external fun stop()
    external fun isRunning(): Boolean

    external fun setVolume(level: Int)               // 1..5
    external fun setPlace(place: Int)                // 0 home, 1 talk, 2 crowd
    external fun setBoost(ear: Int, band: Int, db: Float)
    external fun setBalance(balance: Int)            // -5..5
    external fun setSafety(on: Boolean)
    external fun setSaver(on: Boolean)
    external fun setLowDelay(on: Boolean)
    external fun setMuted(on: Boolean)
    /** Turn down sounds louder than thresholdDb (the wearer's own voice, close to the phone). */
    external fun setOwnVoice(on: Boolean, thresholdDb: Float)
    /** Loudest microphone level (dBFS) in the latest audio block; used to learn the wearer's voice. */
    external fun getInputLevelDb(): Float
    external fun isOwnVoiceActive(): Boolean

    // ---- AI: automatic place selection
    external fun setAutoPlace(on: Boolean)
    /** Place the AI chose: 0 home, 1 talk, 2 crowd. */
    external fun getAutoPlace(): Int
    external fun getNoiseFloorDb(): Float

    // ---- AI: wearer's voice model (18 floats: 9 means then 9 variances)
    external fun setVoiceProfile(profile: FloatArray?)
    external fun startVoiceLearning()
    external fun stopVoiceLearning()
    /** 0 idle, 1 learning, 2 finishing, 3 learned, 4 not enough clear voice */
    external fun getVoiceLearningState(): Int
    external fun getLearnedVoiceProfile(): FloatArray?
    external fun hasVoiceProfile(): Boolean
    /** Above 0: sounds like the wearer. Below 0: someone else. */
    external fun getSpeakerScore(): Float

    external fun playTone(freqHz: Float, ear: Int, levelDb: Float, durationMs: Float)
    external fun stopTone()
    external fun isTonePlaying(): Boolean

    external fun getLevel(): Float
    external fun getLatencyMs(): Float
    external fun isFastPath(): Boolean
    external fun getSampleRate(): Int
    external fun isHowling(): Boolean
    external fun isNoiseReductionActive(): Boolean
}
