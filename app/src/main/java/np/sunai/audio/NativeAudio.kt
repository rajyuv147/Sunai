package np.sunai.audio

/** Thin bridge to the C++ sound engine (see src/main/cpp/jni_bridge.cpp). */
object NativeAudio {
    init {
        System.loadLibrary("sunai")
    }

    /** Device ids from AudioDeviceInfo.id; 0 lets Android choose. */
    external fun start(outputDeviceId: Int, inputDeviceId: Int): Boolean
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
