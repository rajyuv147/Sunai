package np.sunai.ui

import android.content.Intent
import android.os.Bundle
import android.provider.Settings
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import np.sunai.R
import np.sunai.audio.NativeAudio
import np.sunai.core.Sunai
import np.sunai.core.VoiceHelp

/**
 * Connect earphone. Pairing itself happens in Android's Bluetooth screen (no extra
 * permissions needed); this screen shows the steps as pictures and the result.
 */
class ConnectActivity : BaseActivity() {

    override val voiceKey = VoiceHelp.Key.CONNECT

    private var searching = false
    private val dots = mutableListOf<View>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_connect)
        setupBack()
        setupVoiceButton()

        for (id in listOf(R.id.dots_left, R.id.dots_right)) {
            val row = findViewById<LinearLayout>(id)
            repeat(3) {
                val d = View(this)
                val lp = LinearLayout.LayoutParams(Ui.dp(this, 8f), Ui.dp(this, 8f))
                lp.marginStart = Ui.dp(this, 3f); lp.marginEnd = Ui.dp(this, 3f)
                row.addView(d, lp)
                dots += d
            }
        }
        findViewById<View>(R.id.wired_card).background = Ui.round(this, Ui.color(this, R.color.green_soft), 22f)

        findViewById<View>(R.id.connect_btn).setOnClickListener {
            if (Sunai.earphones.state.connected) {
                finish()   // done: back to listening
            } else {
                searching = true
                refresh()
                startActivity(Intent(Settings.ACTION_BLUETOOTH_SETTINGS))
            }
        }
    }

    override fun refresh() {
        val ear = Sunai.earphones.state
        val done = ear.connected
        if (done) searching = false

        val dotColor = Ui.color(this, when {
            done -> R.color.green
            searching -> R.color.blue
            else -> R.color.line
        })
        dots.forEach { it.background = Ui.circle(this, dotColor) }
        findViewById<View>(R.id.bt_badge).background =
            Ui.circle(this, Ui.color(this, if (done) R.color.green else R.color.blue))

        val btn = findViewById<View>(R.id.connect_btn)
        val icon = findViewById<ImageView>(R.id.connect_icon)
        val word = findViewById<TextView>(R.id.connect_word)
        if (done) {
            btn.background = Ui.circle(this, Ui.color(this, R.color.green), Ui.color(this, R.color.green_ring), 12f)
            icon.setImageResource(R.drawable.ic_check)
            word.setText(R.string.connect_done)
        } else {
            val ring = if (searching) R.color.blue_ring else R.color.blue_soft
            btn.background = Ui.circle(this, Ui.color(this, R.color.blue), Ui.color(this, ring), 12f)
            icon.setImageResource(R.drawable.ic_bluetooth)
            word.setText(R.string.connect_btn)
        }
        btn.contentDescription = word.text

        // Speed: decided by the earphone type, confirmed by the measured delay when running.
        val card = findViewById<View>(R.id.speed_card)
        card.visibility = if (done) View.VISIBLE else View.GONE
        if (done) {
            val measured = if (NativeAudio.isRunning()) NativeAudio.getLatencyMs() else -1f
            val fast = if (measured > 0f) measured < FAST_MS else ear.kind.fast
            findViewById<View>(R.id.speed_badge).background =
                Ui.circle(this, Ui.color(this, if (fast) R.color.green else R.color.amber))
            findViewById<ImageView>(R.id.speed_icon)
                .setImageResource(if (fast) R.drawable.ic_bolt else R.drawable.ic_hourglass)
            findViewById<TextView>(R.id.speed_word).setText(if (fast) R.string.speed_fast else R.string.speed_slow)
            findViewById<TextView>(R.id.speed_hint).setText(if (fast) R.string.speed_fast_hint else R.string.speed_slow_hint)
        }
    }

    companion object {
        /** Below this total delay, speech still looks in sync with lips. */
        private const val FAST_MS = 80f
    }
}
