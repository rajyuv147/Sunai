package np.sunai.ui

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import np.sunai.R
import np.sunai.audio.NativeAudio
import np.sunai.core.Sunai
import np.sunai.core.VoiceHelp

/**
 * Ear test that finds how loud each tone must be before it is heard.
 *
 * For each of six pitches the tone plays by itself, starting very soft. If the
 * listener presses red ("didn't hear"), it plays again 10 dB louder, up to six
 * steps. Green ("heard") records that level and moves to the next pitch.
 * The boost for that pitch in that ear is about half the measured loss (the
 * classic "half-gain rule" used in fitting hearing aids).
 * This is a comfort setting, not a medical hearing test.
 */
class TestActivity : BaseActivity() {

    override val voiceKey = VoiceHelp.Key.TEST

    private var ear = 0          // 0 left, 1 right
    private var step = 0         // which pitch, 0..5; 6 = done
    private var levelIndex = 0   // which loudness step for this pitch
    private var started = false
    private val stepDots = mutableListOf<View>()
    private val levelBars = mutableListOf<View>()
    private val handler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_test)
        setupBack()
        setupVoiceButton()

        val dotsRow = findViewById<LinearLayout>(R.id.test_dots)
        repeat(Sunai.BANDS) {
            val d = View(this)
            val size = Ui.dp(this, 28f)
            val lp = LinearLayout.LayoutParams(size, size)
            lp.marginStart = Ui.dp(this, 5f); lp.marginEnd = Ui.dp(this, 5f)
            dotsRow.addView(d, lp)
            stepDots += d
        }
        val barsRow = findViewById<LinearLayout>(R.id.level_bars)
        for (i in LEVELS.indices) {
            val b = View(this)
            val lp = LinearLayout.LayoutParams(Ui.dp(this, 14f), Ui.dp(this, 10f + i * 6f))
            lp.marginStart = Ui.dp(this, 3f); lp.marginEnd = Ui.dp(this, 3f)
            barsRow.addView(b, lp)
            levelBars += b
        }

        findViewById<View>(R.id.ear_left).setOnClickListener { pickEar(0) }
        findViewById<View>(R.id.ear_right).setOnClickListener { pickEar(1) }
        findViewById<View>(R.id.play_btn).setOnClickListener { playTone() }
        findViewById<View>(R.id.yes_btn).setOnClickListener { answer(heard = true) }
        findViewById<View>(R.id.no_btn).setOnClickListener { answer(heard = false) }
        findViewById<View>(R.id.again_btn).setOnClickListener { restart() }
        findViewById<View>(R.id.forward_btn).setOnClickListener { goTo(MainActivity::class.java) }

        findViewById<View>(R.id.no_btn).background = Ui.round(this, Ui.color(this, R.color.red), 28f)
        findViewById<View>(R.id.yes_btn).background = Ui.round(this, Ui.color(this, R.color.green), 28f)
        findViewById<View>(R.id.again_btn).background =
            Ui.round(this, Ui.color(this, R.color.white), 26f, Ui.color(this, R.color.line), 3f)
        findViewById<View>(R.id.forward_btn).background = Ui.round(this, Ui.color(this, R.color.blue), 26f)
        findViewById<View>(R.id.done_badge).background =
            Ui.circle(this, Ui.color(this, R.color.green), Ui.color(this, R.color.green_ring), 14f)
    }

    override fun onResume() {
        super.onResume()
        if (Ui.isDebug(this) && intent.getBooleanExtra("demo", false)) { refresh(); return }
        if (!Sunai.beginTest()) {
            goTo(ConnectActivity::class.java)
            finish()
            return
        }
        started = true
        // First tone plays by itself shortly after the screen opens.
        scheduleTone(900)
    }

    override fun onPause() {
        handler.removeCallbacksAndMessages(null)
        if (started) Sunai.endTest()
        started = false
        super.onPause()
    }

    private fun pickEar(e: Int) {
        ear = e
        restart()
    }

    private fun restart() {
        step = 0
        levelIndex = 0
        NativeAudio.stopTone()
        refresh()
        scheduleTone(600)
    }

    private fun scheduleTone(delayMs: Long) {
        handler.removeCallbacks(autoPlay)
        handler.postDelayed(autoPlay, delayMs)
    }

    private val autoPlay = Runnable { playTone() }

    private fun playTone() {
        if (step >= Sunai.BANDS) return
        NativeAudio.playTone(Sunai.BAND_FREQS[step], ear, LEVELS[levelIndex], TONE_MS)
        handler.removeCallbacks(tonePoll)
        handler.postDelayed(tonePoll, 50)
        stylePlay(true)
    }

    private fun answer(heard: Boolean) {
        if (step >= Sunai.BANDS) return
        NativeAudio.stopTone()
        if (!heard && levelIndex < LEVELS.size - 1) {
            // Not heard: try the same pitch louder.
            levelIndex++
            refresh()
            scheduleTone(400)
            return
        }
        // Heard (or not heard even at the loudest step): record and move on.
        val heardAt = if (heard) LEVELS[levelIndex] else LEVELS.last() + 10f
        val loss = heardAt - LEVELS[0]
        Sunai.saveBoost(ear, step, (loss * 0.5f).coerceIn(0f, MAX_BOOST_DB))
        step++
        levelIndex = 0
        if (step >= Sunai.BANDS) {
            Sunai.prefs.testDone = true
            Sunai.voice.say(VoiceHelp.Key.TEST_DONE)
        } else {
            scheduleTone(700)
        }
        refresh()
    }

    override fun refresh() {
        val done = step >= Sunai.BANDS
        findViewById<View>(R.id.test_group).visibility = if (done) View.GONE else View.VISIBLE
        findViewById<View>(R.id.done_group).visibility = if (done) View.VISIBLE else View.GONE

        for ((id, index) in listOf(R.id.ear_left to 0, R.id.ear_right to 1)) {
            val tile = findViewById<LinearLayout>(id)
            val sel = index == ear
            tile.background = if (sel) Ui.round(this, Ui.color(this, R.color.blue), 26f)
                else Ui.round(this, Ui.color(this, R.color.white), 26f, Ui.color(this, R.color.card_line), 1.5f)
            val fg = Ui.color(this, if (sel) R.color.white else R.color.muted)
            Ui.tint(tile.getChildAt(0) as ImageView, fg)
            (tile.getChildAt(1) as TextView).setTextColor(fg)
            tile.isSelected = sel
        }

        stepDots.forEachIndexed { i, d ->
            d.background = when {
                i < step -> Ui.circle(this, Ui.color(this, R.color.green))
                i == step -> Ui.circle(this, Ui.color(this, R.color.white), Ui.color(this, R.color.blue), 5f)
                else -> Ui.circle(this, Ui.color(this, R.color.line))
            }
        }
        findViewById<View>(R.id.test_dots).contentDescription = "${minOf(step + 1, Sunai.BANDS)} / ${Sunai.BANDS}"
        levelBars.forEachIndexed { i, b ->
            b.background = Ui.round(this, Ui.color(this, if (i <= levelIndex) R.color.blue else R.color.card_line), 4f)
        }
        stylePlay(NativeAudio.isTonePlaying())
    }

    private fun stylePlay(playing: Boolean) {
        val btn = findViewById<View>(R.id.play_btn)
        btn.background = if (playing)
            Ui.circle(this, Ui.color(this, R.color.blue), Ui.color(this, R.color.blue_ring), 14f)
        else
            Ui.circle(this, Ui.color(this, R.color.white), Ui.color(this, R.color.blue_soft), 14f)
        Ui.tint(findViewById(R.id.play_icon), Ui.color(this, if (playing) R.color.white else R.color.blue))
    }

    private val tonePoll = object : Runnable {
        override fun run() {
            val playing = NativeAudio.isTonePlaying()
            stylePlay(playing)
            if (playing) handler.postDelayed(this, 100)
        }
    }

    companion object {
        /** Tone loudness steps in dBFS, from very soft to very loud (10 dB apart). */
        private val LEVELS = floatArrayOf(-60f, -50f, -40f, -30f, -20f, -10f)
        private const val TONE_MS = 1200f
        private const val MAX_BOOST_DB = 30f
    }
}
