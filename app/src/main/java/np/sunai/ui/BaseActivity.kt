package np.sunai.ui

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import np.sunai.R
import np.sunai.core.Sunai
import np.sunai.core.VoiceHelp

/** Shared behaviour: amber voice-help button, bottom navigation, live refresh. */
abstract class BaseActivity : Activity() {

    /** What the voice-help button says on this screen. */
    protected abstract val voiceKey: VoiceHelp.Key

    /** Redraw the screen from the current state. */
    protected abstract fun refresh()

    private val onChange: () -> Unit = { runOnUiThread { refresh() } }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Sunai.init(this)
    }

    override fun onResume() {
        super.onResume()
        Sunai.addListener(onChange)
        Sunai.voice.onSpeakingChanged = { speaking -> runOnUiThread { styleVoiceButton(speaking) } }
        styleVoiceButton(false)
        refresh()
    }

    override fun onPause() {
        Sunai.removeListener(onChange)
        Sunai.voice.stop()
        super.onPause()
    }

    // ---------------------------------------------------------------- voice help

    protected fun setupVoiceButton() {
        val btn = findViewById<View>(R.id.voice_btn) ?: return
        btn.setOnClickListener {
            if (Sunai.voice.speaking) Sunai.voice.stop() else Sunai.voice.say(voiceKey)
        }
    }

    private fun styleVoiceButton(speaking: Boolean) {
        val btn = findViewById<View>(R.id.voice_btn) ?: return
        val icon = findViewById<ImageView>(R.id.voice_icon)
        val label = findViewById<TextView>(R.id.voice_label)
        val fill = Ui.color(this, if (speaking) R.color.amber else R.color.amber_soft)
        val ring = Ui.color(this, if (speaking) R.color.amber_ring else R.color.amber_soft)
        val fg = Ui.color(this, if (speaking) R.color.white else R.color.amber_text)
        btn.background = Ui.circle(this, fill, ring, 4f)
        Ui.tint(icon, fg)
        label.setTextColor(fg)
        label.setText(if (speaking) R.string.voice_btn_speaking else R.string.voice_btn)
    }

    // ---------------------------------------------------------------- navigation

    protected fun setupNav(selected: Int) {
        val items = listOf(
            Triple(R.id.nav_listen, MainActivity::class.java, 0),
            Triple(R.id.nav_test, TestActivity::class.java, 1),
            Triple(R.id.nav_helper, HelperActivity::class.java, 2),
        )
        for ((id, target, index) in items) {
            val item = findViewById<LinearLayout>(id) ?: continue
            val on = index == selected
            item.background = if (on) Ui.round(this, Ui.color(this, R.color.green_soft), 18f) else null
            val color = Ui.color(this, if (on) R.color.green_text else R.color.nav_idle)
            (item.getChildAt(0) as ImageView).let { Ui.tint(it, color) }
            (item.getChildAt(1) as TextView).setTextColor(color)
            item.isSelected = on
            item.setOnClickListener { if (!on) goTo(target) }
        }
    }

    protected fun setupBack() {
        findViewById<View>(R.id.back)?.setOnClickListener { finish() }
    }

    protected fun goTo(target: Class<out Activity>) {
        startActivity(Intent(this, target).addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT))
        @Suppress("DEPRECATION")
        overridePendingTransition(0, 0)
    }
}
