package np.sunai.core

import android.content.Context
import android.content.SharedPreferences

/** All saved settings. Stored only on the phone (SharedPreferences, built into Android). */
class Prefs(context: Context) {
    private val sp: SharedPreferences =
        context.getSharedPreferences("sunai", Context.MODE_PRIVATE)

    var volume: Int
        get() = sp.getInt("volume", 3)
        set(v) = sp.edit().putInt("volume", v.coerceIn(1, 5)).apply()

    /** 0 home, 1 talk, 2 crowd */
    var place: Int
        get() = sp.getInt("place", 1)
        set(v) = sp.edit().putInt("place", v.coerceIn(0, 2)).apply()

    var balance: Int
        get() = sp.getInt("balance", 0)
        set(v) = sp.edit().putInt("balance", v.coerceIn(-5, 5)).apply()

    var safety: Boolean
        get() = sp.getBoolean("safety", true)
        set(v) = sp.edit().putBoolean("safety", v).apply()

    /** Battery saver switched on by the helper (separate from the automatic low-battery saver). */
    var saverManual: Boolean
        get() = sp.getBoolean("saver", false)
        set(v) = sp.edit().putBoolean("saver", v).apply()

    var lowDelay: Boolean
        get() = sp.getBoolean("low_delay", false)
        set(v) = sp.edit().putBoolean("low_delay", v).apply()

    /** Voice-help language: ne, mai, bho, tha */
    var voiceLang: String
        get() = sp.getString("voice_lang", "ne") ?: "ne"
        set(v) = sp.edit().putString("voice_lang", v).apply()

    /** Ear-test boost in dB for one ear (0 left, 1 right) and band (0..5). */
    fun boost(ear: Int, band: Int): Float = sp.getFloat("boost_${ear}_$band", 0f)

    fun setBoost(ear: Int, band: Int, db: Float) =
        sp.edit().putFloat("boost_${ear}_$band", db).apply()

    /** Own-voice reduction on/off and its threshold (dBFS), learned from the wearer's voice. */
    var ownVoice: Boolean
        get() = sp.getBoolean("own_voice", true)
        set(v) = sp.edit().putBoolean("own_voice", v).apply()

    var ownVoiceDb: Float
        get() = sp.getFloat("own_voice_db", -26f)
        set(v) = sp.edit().putFloat("own_voice_db", v.coerceIn(-50f, -6f)).apply()

    /** Let the AI choose Home / Talk / Crowd. */
    var autoPlace: Boolean
        get() = sp.getBoolean("auto_place", true)
        set(v) = sp.edit().putBoolean("auto_place", v).apply()

    /** The wearer's learned voice model (18 numbers), or null if not learned yet. */
    var voiceProfile: FloatArray?
        get() = sp.getString("voice_profile", null)
            ?.split(',')?.mapNotNull { it.toFloatOrNull() }
            ?.takeIf { it.size == 18 }?.toFloatArray()
        set(v) = sp.edit().putString("voice_profile", v?.joinToString(",")).apply()

    /** Audio settings level (0 fastest … 3 safest) that last worked on this phone. */
    var audioLevel: Int
        get() = sp.getInt("audio_level", 0)
        set(v) = sp.edit().putInt("audio_level", v.coerceIn(0, 3)).apply()

    var testDone: Boolean
        get() = sp.getBoolean("test_done", false)
        set(v) = sp.edit().putBoolean("test_done", v).apply()
}
