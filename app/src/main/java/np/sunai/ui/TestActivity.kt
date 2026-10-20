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
 * Simple ear test: six tones per ear. A tone that is not heard gets a boost in
 * that ear's equaliser. This is a comfort setting, not a medical hearing test.
 */
class TestActivity : BaseActivity() {

    override val voiceKey = VoiceHelp.Key.TEST

    private var ear = 0          // 0 left, 1 right
    private var step = 0         // 0..5, 6 = done
    private val stepDots = mutableListOf<View>()
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

        findViewById<View>(R.id.ear_left).setOnClickListener { pickEar(0) }
        findViewById<View>(R.id.ear_right).setOnClickListener { pickEar(1) }
        findViewById<View>(R.id.play_btn).setOnClickListener { playTone() }
        findViewById<View>(R.id.yes_btn).setOnClickListener { answer(heard = true) }
        findViewById<View>(R.id.no_btn).setOnClickListener { answer(heard = false) }
        findViewById<View>(R.id.again_btn).setOnClickListener { step = 0; refresh() }
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
        if (!Sunai.beginTest()) {
            goTo(ConnectActivity::class.java)
            finish()
        }
    }

    override fun onPause() {
        handler.removeCallbacks(tonePoll)
        Sunai.endTest()
        super.onPause()
    }

    private fun pickEar(e: Int) {
        ear = e
        step = 0
        NativeAudio.stopTone()
        refresh()
    }

    private fun playTone() {
        if (step >= Sunai.BANDS) return
        NativeAudio.playTone(Sunai.BAND_FREQS[step], ear, TEST_LEVEL_DB, TONE_MS)
        handler.removeCallbacks(tonePoll)
        handler.postDelayed(tonePoll, 50)
        stylePlay(true)
    }

    private fun answer(heard: Boolean) {
        if (step >= Sunai.BANDS) return
        NativeAudio.stopTone()
        // Not heard at the soft test level -> lift that band for this ear.
        Sunai.saveBoost(ear, step, if (heard) 0f else BOOST_DB)
        step++
        if (step >= Sunai.BANDS) {
            Sunai.prefs.testDone = true
            Sunai.voice.say(VoiceHelp.Key.TEST_DONE)
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
            tile.background = Ui.round(this, Ui.color(this, if (sel) R.color.blue else R.color.white), 26f)
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
        /** Soft but clearly audible to normal hearing on typical earphones. */
        private const val TEST_LEVEL_DB = -42f
        private const val TONE_MS = 1200f
        private const val BOOST_DB = 12f
    }
}
