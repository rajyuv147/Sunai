package np.sunai.ui

import android.annotation.SuppressLint
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import np.sunai.R
import np.sunai.audio.NativeAudio
import np.sunai.core.Sunai
import np.sunai.core.VoiceHelp

/** Settings for a family member. Locked behind a 3-second press so they are not changed by accident. */
class HelperActivity : BaseActivity() {

    override val voiceKey = VoiceHelp.Key.HELPER

    private var locked = true
    private val handler = Handler(Looper.getMainLooper())
    private val langs by lazy {
        listOf("ne" to R.id.lang_ne, "mai" to R.id.lang_mai, "bho" to R.id.lang_bho, "tha" to R.id.lang_tha)
    }

    private class Row(val root: View) {
        val track: FrameLayout = root.findViewById(R.id.row_track)
        val knob: View = root.findViewById(R.id.row_knob)
    }
    private lateinit var lowDelay: Row
    private lateinit var safety: Row
    private lateinit var saver: Row
    private lateinit var ownVoice: Row
    private lateinit var extraLoud: Row
    private var learning = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_helper)
        setupBack()
        setupVoiceButton()
        setupNav(2)

        setupUnlock()
        findViewById<View>(R.id.lock_again).setOnClickListener { locked = true; refresh() }
        findViewById<View>(R.id.lock_again).background =
            Ui.round(this, Ui.color(this, R.color.white), 20f, Ui.color(this, R.color.ink), 3f)

        langs.forEach { (code, id) -> findViewById<View>(id).setOnClickListener { Sunai.setVoiceLang(code) } }

        findViewById<View>(R.id.bal_left).setOnClickListener { Sunai.setBalance(Sunai.prefs.balance - 1) }
        findViewById<View>(R.id.bal_right).setOnClickListener { Sunai.setBalance(Sunai.prefs.balance + 1) }
        for (id in listOf(R.id.bal_left, R.id.bal_right)) {
            findViewById<View>(id).background =
                Ui.circle(this, Ui.color(this, R.color.white), Ui.color(this, R.color.blue), 3f)
        }
        findViewById<View>(R.id.bal_line).background = Ui.round(this, Ui.color(this, R.color.track), 7f)
        findViewById<View>(R.id.bal_knob).background =
            Ui.circle(this, Ui.color(this, R.color.blue), Ui.color(this, R.color.white), 4f)

        extraLoud = row(R.id.row_extra_loud, R.drawable.ic_vol_up, R.color.blue, R.color.blue_soft,
            R.string.helper_extra_loud, R.string.helper_extra_loud_sub) { Sunai.setExtraLoud(!Sunai.prefs.extraLoud) }
        lowDelay = row(R.id.row_low_delay, R.drawable.ic_bolt, R.color.green, R.color.green_soft,
            R.string.helper_low_delay, R.string.helper_low_delay_sub) { Sunai.setLowDelay(!Sunai.prefs.lowDelay) }
        safety = row(R.id.row_safety, R.drawable.ic_shield, R.color.blue, R.color.blue_soft,
            R.string.helper_safety, R.string.helper_safety_sub) { Sunai.setSafety(!Sunai.prefs.safety) }
        ownVoice = row(R.id.row_own_voice, R.drawable.ic_place_talk, R.color.blue, R.color.blue_soft,
            R.string.helper_own_voice, R.string.helper_own_voice_sub) { Sunai.setOwnVoice(!Sunai.prefs.ownVoice) }
        findViewById<View>(R.id.learn_voice).setOnClickListener { learnVoice() }
        styleLearn()

        saver = row(R.id.row_saver, R.drawable.ic_battery, R.color.amber, R.color.amber_soft,
            R.string.helper_saver, R.string.helper_saver_sub) { Sunai.setSaverManual(!Sunai.prefs.saverManual) }
    }

    private fun row(id: Int, icon: Int, fg: Int, bg: Int, title: Int, sub: Int, onToggle: () -> Unit): Row {
        val root = findViewById<View>(id)
        root.findViewById<View>(R.id.row_icon_bg).background = Ui.circle(this, Ui.color(this, bg))
        root.findViewById<ImageView>(R.id.row_icon).apply { setImageResource(icon); Ui.tint(this, Ui.color(this@HelperActivity, fg)) }
        root.findViewById<TextView>(R.id.row_title).setText(title)
        root.findViewById<TextView>(R.id.row_sub).setText(sub)
        val r = Row(root)
        r.track.contentDescription = getString(title)
        r.track.setOnClickListener { onToggle() }
        return r
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setupUnlock() {
        val btn = findViewById<TextView>(R.id.unlock_btn)
        btn.background = Ui.round(this, Ui.color(this, R.color.ink), 22f)
        val unlock = Runnable { locked = false; refresh() }
        btn.setOnTouchListener { v, e ->
            when (e.action) {
                MotionEvent.ACTION_DOWN -> {
                    v.background = Ui.round(this, Ui.color(this, R.color.nav_idle), 22f)
                    handler.postDelayed(unlock, UNLOCK_MS)
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    v.background = Ui.round(this, Ui.color(this, R.color.ink), 22f)
                    handler.removeCallbacks(unlock)
                }
            }
            true
        }
        // Screen-reader users can unlock with a long press (double-tap and hold).
        btn.setOnLongClickListener { locked = false; refresh(); true }
    }

    override fun onPause() {
        handler.removeCallbacks(statusTick)
        locked = true   // always locked again when the helper leaves
        super.onPause()
    }

    override fun refresh() {
        findViewById<View>(R.id.locked_group).visibility = if (locked) View.VISIBLE else View.GONE
        findViewById<View>(R.id.unlocked_group).visibility = if (locked) View.GONE else View.VISIBLE
        if (locked) return

        langs.forEach { (code, id) ->
            val chip = findViewById<TextView>(id)
            val sel = Sunai.prefs.voiceLang == code
            chip.background = if (sel) Ui.round(this, Ui.color(this, R.color.amber), 16f)
                else Ui.round(this, Ui.color(this, R.color.ground), 16f, Ui.color(this, R.color.track), 2f)
            chip.setTextColor(Ui.color(this, if (sel) R.color.white else R.color.ink))
            chip.isSelected = sel
        }

        // Balance knob: centre = equal, each step moves 10 % of the track.
        val track = findViewById<View>(R.id.bal_track)
        val knob = findViewById<View>(R.id.bal_knob)
        track.post {
            val usable = track.width - knob.width
            knob.translationX = usable / 2f + usable * 0.1f * Sunai.prefs.balance
        }

        Ui.setToggle(lowDelay.track, lowDelay.knob, Sunai.prefs.lowDelay)
        Ui.setToggle(safety.track, safety.knob, Sunai.prefs.safety)
        Ui.setToggle(saver.track, saver.knob, Sunai.saverOn)
        Ui.setToggle(ownVoice.track, ownVoice.knob, Sunai.prefs.ownVoice)
        Ui.setToggle(extraLoud.track, extraLoud.knob, Sunai.prefs.extraLoud)

        drawStatus()
    }

    /** Live engine numbers for the helper (and for bug reports: take a screenshot). */
    private fun drawStatus() {
        val status = findViewById<TextView>(R.id.status_text)
        val ear = Sunai.earphones.state
        val sb = StringBuilder()
        sb.append(getString(R.string.helper_status)).append(": ")
        sb.append(if (Sunai.listening) "ON" else "OFF")
        sb.append(" · ").append(ear.kind.name).append(" #").append(ear.outputId)
        sb.append(" · mic #").append(ear.micId)
        if (NativeAudio.isRunning()) {
            val ms = NativeAudio.getLatencyMs()
            sb.append("\n").append(if (NativeAudio.isFastPath()) "⚡ " else "⏳ ")
            sb.append(if (ms > 0) "${ms.toInt()} ms" else "? ms")
            sb.append(" · AI ").append(if (NativeAudio.isNoiseReductionActive()) "✓" else "–")
        }
        if (NativeAudio.isRunning()) {
            val places = arrayOf("घर", "कुरा", "भीड")
            sb.append("\nAI: ")
            sb.append(if (Sunai.prefs.autoPlace) "स्वतः→" else "").append(places[Sunai.currentPlace])
            sb.append(" (floor ${NativeAudio.getNoiseFloorDb().toInt()} dB)")
            if (NativeAudio.hasVoiceProfile()) {
                val score = NativeAudio.getSpeakerScore()
                sb.append(" · voice ✓ ").append(if (score > 0) "me" else if (score < -2) "other" else "?")
            } else {
                sb.append(" · voice –")
            }
        }
        sb.append("\n").append(NativeAudio.getDiagnostics())
        status.text = sb.toString()
        status.visibility = View.VISIBLE
    }

    private val statusTick = object : Runnable {
        override fun run() {
            if (!locked) drawStatus()
            handler.postDelayed(this, 700)
        }
    }

    override fun onResume() {
        super.onResume()
        handler.post(statusTick)
    }

    // ---------------------------------------------------------------- learn my voice

    /**
     * The wearer talks normally for 5 seconds with the phone where it is usually kept.
     * Two things are learned:
     *  - their loud speech level -> own-voice threshold (10 dB below it)
     *  - their voice model (pitch + voice colour), so a loud visitor is not turned down
     */
    private fun learnVoice() {
        if (learning) return
        if (!NativeAudio.isRunning()) {
            Toast.makeText(this, R.string.helper_need_listening, Toast.LENGTH_LONG).show()
            return
        }
        learning = true
        styleLearn()
        NativeAudio.startVoiceLearning()
        val samples = ArrayList<Float>()
        val start = System.currentTimeMillis()
        val text = findViewById<TextView>(R.id.learn_text)
        val tick = object : Runnable {
            override fun run() {
                val elapsed = System.currentTimeMillis() - start
                val level = NativeAudio.getInputLevelDb()
                if (level > -90f) samples += level
                if (elapsed < LEARN_MS) {
                    val left = ((LEARN_MS - elapsed) / 1000 + 1).toInt()
                    text.text = getString(R.string.helper_learn_listening, left)
                    handler.postDelayed(this, 50)
                } else {
                    finishLearning(samples)
                }
            }
        }
        handler.post(tick)
    }

    private fun finishLearning(samples: List<Float>) {
        NativeAudio.stopVoiceLearning()
        // The sound engine finishes the voice model on its next audio block.
        var tries = 0
        val waitModel = object : Runnable {
            override fun run() {
                val state = NativeAudio.getVoiceLearningState()
                if (state == 2 && ++tries < 20) { handler.postDelayed(this, 25); return }
                applyLearning(samples, if (state == 3) NativeAudio.getLearnedVoiceProfile() else null)
            }
        }
        handler.post(waitModel)
    }

    private fun applyLearning(samples: List<Float>, profile: FloatArray?) {
        learning = false
        val sorted = samples.sorted()
        val loud = if (sorted.size >= 20) sorted[(sorted.size * 0.9f).toInt().coerceAtMost(sorted.size - 1)] else -120f
        if (loud < -50f) {
            Toast.makeText(this, R.string.helper_learn_quiet, Toast.LENGTH_LONG).show()
        } else {
            val threshold = (loud - 10f).coerceIn(-38f, -12f)
            Sunai.setOwnVoiceDb(threshold)
            if (!Sunai.prefs.ownVoice) Sunai.setOwnVoice(true)
            if (profile != null) Sunai.saveVoiceProfile(profile)
            val msg = if (profile != null) getString(R.string.helper_learn_done_voice, threshold.toInt())
                      else getString(R.string.helper_learn_done, threshold.toInt())
            Toast.makeText(this, msg, Toast.LENGTH_LONG).show()
        }
        styleLearn()
        refresh()
    }

    private fun styleLearn() {
        val btn = findViewById<View>(R.id.learn_voice)
        val fill = Ui.color(this, if (learning) R.color.blue else R.color.blue_soft)
        val fg = Ui.color(this, if (learning) R.color.white else R.color.blue)
        btn.background = Ui.round(this, fill, 18f)
        Ui.tint(findViewById(R.id.learn_icon), fg)
        findViewById<TextView>(R.id.learn_text).apply {
            setTextColor(fg)
            if (!learning) setText(R.string.helper_learn_voice)
        }
        findViewById<TextView>(R.id.learn_sub).setTextColor(
            Ui.color(this, if (learning) R.color.white else R.color.muted))
    }

    companion object {
        private const val LEARN_MS = 5000L
        private const val UNLOCK_MS = 3000L
    }
}
