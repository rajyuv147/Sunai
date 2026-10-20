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

        lowDelay = row(R.id.row_low_delay, R.drawable.ic_bolt, R.color.green, R.color.green_soft,
            R.string.helper_low_delay, R.string.helper_low_delay_sub) { Sunai.setLowDelay(!Sunai.prefs.lowDelay) }
        safety = row(R.id.row_safety, R.drawable.ic_shield, R.color.blue, R.color.blue_soft,
            R.string.helper_safety, R.string.helper_safety_sub) { Sunai.setSafety(!Sunai.prefs.safety) }
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

        // Diagnostics for the helper, only while sound is running.
        val status = findViewById<TextView>(R.id.status_text)
        if (NativeAudio.isRunning()) {
            val ms = NativeAudio.getLatencyMs()
            val delay = if (ms > 0) "${ms.toInt()} ms" else "?"
            val path = if (NativeAudio.isFastPath()) "⚡" else "⏳"
            val ai = if (NativeAudio.isNoiseReductionActive()) "AI ✓" else "AI –"
            status.text = "${getString(R.string.helper_status)}: $path $delay · ${NativeAudio.getSampleRate()} Hz · $ai"
            status.visibility = View.VISIBLE
        } else {
            status.visibility = View.GONE
        }
    }

    companion object {
        private const val UNLOCK_MS = 3000L
    }
}
