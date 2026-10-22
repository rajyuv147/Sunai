package np.sunai.core

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import np.sunai.audio.ListenService
import np.sunai.audio.NativeAudio

/**
 * The app's single controller: owns settings, the earphone monitor, voice help,
 * the automatic battery saver and the listening on/off state. Screens only talk to this.
 */
object Sunai {
    const val BANDS = 6
    val BAND_FREQS = floatArrayOf(250f, 500f, 1000f, 2000f, 4000f, 8000f)

    lateinit var prefs: Prefs private set
    lateinit var earphones: EarphoneMonitor private set
    lateinit var voice: VoiceHelp private set
    private lateinit var app: Context
    private var initialised = false

    /** True while sound is going from the mic to the earphone. */
    var listening = false
        private set
    /** True while the ear test owns the engine. */
    private var testing = false

    /** Battery saver switched on automatically because the battery is low. */
    var autoSaver = false
        private set
    val saverOn: Boolean get() = prefs.saverManual || autoSaver

    private val listeners = mutableListOf<() -> Unit>()
    private val main = Handler(Looper.getMainLooper())

    fun init(context: Context) {
        if (initialised) return
        initialised = true
        app = context.applicationContext
        prefs = Prefs(app)
        earphones = EarphoneMonitor(app)
        voice = VoiceHelp(app, prefs)
        earphones.addListener { onEarphoneChanged(it) }
        watchBattery()
    }

    fun addListener(l: () -> Unit) { listeners += l }
    fun removeListener(l: () -> Unit) { listeners -= l }
    private fun notifyChanged() = listeners.toList().forEach { it() }

    // ---------------------------------------------------------------- listening

    /** Returns false (and asks for an earphone) if none is connected. */
    fun startListening(): Boolean {
        if (!earphones.state.connected) {
            voice.say(VoiceHelp.Key.NO_EARPHONE)
            return false
        }
        listening = true
        ListenService.start(app)
        loudMinutes = 0
        main.removeCallbacks(loudCheck)
        main.postDelayed(loudCheck, 60_000)
        notifyChanged()
        return true
    }

    fun stopListening() {
        listening = false
        main.removeCallbacks(loudCheck)
        if (!testing) ListenService.stop(app)
        notifyChanged()
    }

    /** Called by ListenService once it is in the foreground. */
    internal fun openEngine(): Boolean = startEngine()

    /** Opens mic + earphone at the settings level that last worked on this phone. */
    private fun startEngine(level: Int = prefs.audioLevel): Boolean {
        val s = earphones.state
        val ok = NativeAudio.start(s.outputId, s.micId, level)
        applyAll()
        if (testing) NativeAudio.setMuted(true)
        if (ok) {
            stallChecks = 0
            main.removeCallbacks(micWatchdog)
            main.postDelayed(micWatchdog, 1500)
        }
        return ok
    }

    // ---------------------------------------------------------------- mic watchdog

    private var stallChecks = 0

    /**
     * Some phones accept the fastest microphone settings but then deliver no sound.
     * If the mic stays silent, reopen with the next safer settings and remember
     * the level that works so the next start is right first time.
     */
    private val micWatchdog = object : Runnable {
        override fun run() {
            if (!NativeAudio.isRunning()) return
            if (NativeAudio.isMicStalled()) {
                val next = NativeAudio.getFallbackLevel() + 1
                if (next <= MAX_AUDIO_LEVEL) {
                    startEngine(next)
                    return
                }
            } else if (++stallChecks == 2) {
                // Mic data is flowing: this level works on this phone.
                prefs.audioLevel = NativeAudio.getFallbackLevel()
            }
            main.postDelayed(this, 1500)
        }
    }

    private const val MAX_AUDIO_LEVEL = 3

    // ---------------------------------------------------------------- ear test

    fun beginTest(): Boolean {
        if (!earphones.state.connected) {
            voice.say(VoiceHelp.Key.NO_EARPHONE)
            return false
        }
        testing = true
        NativeAudio.setMuted(true)            // only test tones, no microphone sound
        if (!NativeAudio.isRunning()) startEngine()
        return true
    }

    fun endTest() {
        if (!testing) return
        testing = false
        NativeAudio.stopTone()
        NativeAudio.setMuted(false)
        if (!listening) {
            NativeAudio.stop()
            ListenService.stop(app)
        }
    }

    // ---------------------------------------------------------------- settings

    fun setVolume(v: Int) { prefs.volume = v; NativeAudio.setVolume(prefs.volume); notifyChanged() }
    fun setPlace(p: Int) { prefs.place = p; NativeAudio.setPlace(prefs.place); notifyChanged() }
    fun setBalance(b: Int) { prefs.balance = b; NativeAudio.setBalance(prefs.balance); notifyChanged() }
    fun setSafety(on: Boolean) { prefs.safety = on; NativeAudio.setSafety(on); notifyChanged() }
    fun setLowDelay(on: Boolean) { prefs.lowDelay = on; NativeAudio.setLowDelay(on); notifyChanged() }
    fun setSaverManual(on: Boolean) { prefs.saverManual = on; NativeAudio.setSaver(saverOn); notifyChanged() }
    fun setVoiceLang(lang: String) { prefs.voiceLang = lang; notifyChanged() }

    fun saveBoost(ear: Int, band: Int, db: Float) {
        prefs.setBoost(ear, band, db)
        NativeAudio.setBoost(ear, band, db)
    }

    /** Pushes every saved setting into the sound engine. */
    fun applyAll() {
        NativeAudio.setVolume(prefs.volume)
        NativeAudio.setPlace(prefs.place)
        NativeAudio.setBalance(prefs.balance)
        NativeAudio.setSafety(prefs.safety)
        NativeAudio.setLowDelay(prefs.lowDelay)
        NativeAudio.setSaver(saverOn)
        for (ear in 0..1) for (b in 0 until BANDS) NativeAudio.setBoost(ear, b, prefs.boost(ear, b))
    }

    // ---------------------------------------------------------------- earphones

    private fun onEarphoneChanged(s: EarphoneMonitor.State) {
        if (s.connected) {
            voice.say(VoiceHelp.Key.CONNECTED)
            // Re-open on the new (possibly faster) earphone.
            if (NativeAudio.isRunning()) startEngine()
        } else if (listening) {
            // Never play amplified sound through the loudspeaker: it would squeal.
            stopListening()
            voice.say(VoiceHelp.Key.NO_EARPHONE)
        }
        notifyChanged()
    }

    // ---------------------------------------------------------------- battery saver

    private fun watchBattery() {
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                val level = i.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
                val scale = i.getIntExtra(BatteryManager.EXTRA_SCALE, 100)
                val status = i.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
                val charging = status == BatteryManager.BATTERY_STATUS_CHARGING ||
                    status == BatteryManager.BATTERY_STATUS_FULL
                if (level < 0 || scale <= 0) return
                val pct = level * 100 / scale
                val wasOn = autoSaver
                // Turn on at 20 %, off again at 25 % or when charging (avoids flicker).
                autoSaver = when {
                    charging -> false
                    pct <= 20 -> true
                    pct >= 25 -> false
                    else -> autoSaver
                }
                if (autoSaver != wasOn) {
                    NativeAudio.setSaver(saverOn)
                    if (autoSaver && listening) voice.say(VoiceHelp.Key.SAVER_ON)
                    notifyChanged()
                }
            }
        }
        val filter = IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        if (Build.VERSION.SDK_INT >= 33) {
            app.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            app.registerReceiver(receiver, filter)
        }
    }

    // ---------------------------------------------------------------- ear safety

    private var loudMinutes = 0
    private val loudCheck = object : Runnable {
        override fun run() {
            if (!listening) return
            loudMinutes = if (prefs.volume >= 5) loudMinutes + 1 else 0
            // After an hour at the top volume, remind the listener to rest their ears.
            if (loudMinutes >= 60) {
                voice.say(VoiceHelp.Key.LOUD)
                loudMinutes = 0
            }
            main.postDelayed(this, 60_000)
        }
    }
}
