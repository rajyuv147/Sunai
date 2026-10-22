package np.sunai.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import np.sunai.R
import np.sunai.audio.NativeAudio
import np.sunai.core.Sunai
import np.sunai.core.VoiceHelp

/** Home: the big ear button, volume and place. */
class MainActivity : BaseActivity() {

    override val voiceKey = VoiceHelp.Key.HOME

    private lateinit var bigButton: FrameLayout
    private lateinit var bigIcon: ImageView
    private lateinit var stateWord: TextView
    private lateinit var meter: LinearLayout
    private lateinit var volBars: LinearLayout
    private val meterBars = mutableListOf<View>()
    private val volumeBars = mutableListOf<View>()
    private val places by lazy {
        listOf(findViewById<LinearLayout>(R.id.place_home),
               findViewById<LinearLayout>(R.id.place_talk),
               findViewById<LinearLayout>(R.id.place_crowd))
    }

    private val autoTile by lazy { findViewById<LinearLayout>(R.id.place_auto) }
    private var shownPlace = -1

    private val handler = Handler(Looper.getMainLooper())
    private var resumed = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bigButton = findViewById(R.id.big_button)
        bigIcon = findViewById(R.id.big_icon)
        stateWord = findViewById(R.id.state_word)
        meter = findViewById(R.id.meter)
        volBars = findViewById(R.id.vol_bars)

        buildMeter()
        buildVolumeBars()
        setupVoiceButton()
        setupNav(0)

        bigButton.setOnClickListener { toggleListening() }
        findViewById<View>(R.id.vol_down).setOnClickListener { Sunai.setVolume(Sunai.prefs.volume - 1) }
        findViewById<View>(R.id.vol_up).setOnClickListener { Sunai.setVolume(Sunai.prefs.volume + 1) }
        places.forEachIndexed { i, tile -> tile.setOnClickListener { Sunai.pickPlace(i) } }
        autoTile.setOnClickListener { Sunai.setAutoPlace(true) }
        findViewById<View>(R.id.earphone_pill).setOnClickListener { goTo(ConnectActivity::class.java) }

        // Static backgrounds
        findViewById<View>(R.id.earphone_pill).background = Ui.round(this, Ui.color(this, R.color.white), 28f)
        findViewById<View>(R.id.vol_down).background =
            Ui.circle(this, Ui.color(this, R.color.white), Ui.color(this, R.color.blue), 3f)
        findViewById<View>(R.id.vol_up).background = Ui.circle(this, Ui.color(this, R.color.blue))
    }

    override fun onResume() {
        super.onResume()
        resumed = true
        handler.post(meterTick)
    }

    override fun onPause() {
        resumed = false
        handler.removeCallbacks(meterTick)   // no screen updates while not visible: saves battery
        super.onPause()
    }

    // ---------------------------------------------------------------- listening

    private fun toggleListening() {
        if (Sunai.listening) { Sunai.stopListening(); return }
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            val perms = mutableListOf(Manifest.permission.RECORD_AUDIO)
            if (Build.VERSION.SDK_INT >= 33) perms += Manifest.permission.POST_NOTIFICATIONS
            requestPermissions(perms.toTypedArray(), REQ_MIC)
            return
        }
        if (!Sunai.startListening()) goTo(ConnectActivity::class.java)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQ_MIC &&
            checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            if (!Sunai.startListening()) goTo(ConnectActivity::class.java)
        }
    }

    // ---------------------------------------------------------------- drawing

    override fun refresh() {
        val on = Sunai.listening
        bigButton.background = if (on)
            Ui.circle(this, Ui.color(this, R.color.green), Ui.color(this, R.color.green_ring), 14f)
        else
            Ui.circle(this, Ui.color(this, R.color.white), Ui.color(this, R.color.red_ring), 14f)
        if (on) {
            bigIcon.setImageResource(R.drawable.ic_ear_on)
            Ui.tint(bigIcon, Ui.color(this, R.color.white))
        } else {
            bigIcon.setImageResource(R.drawable.ic_ear_off)   // two-colour icon, not tinted
            bigIcon.imageTintList = null
        }
        bigButton.contentDescription = getString(if (on) R.string.a11y_turn_off else R.string.a11y_turn_on)
        stateWord.setText(if (on) R.string.state_on else R.string.state_off)
        stateWord.setTextColor(Ui.color(this, if (on) R.color.green else R.color.red))

        // Volume bars
        val vol = Sunai.prefs.volume
        volumeBars.forEachIndexed { i, bar ->
            bar.background = Ui.round(this, Ui.color(this, if (i < vol) R.color.blue else R.color.track), 8f)
        }
        volBars.contentDescription = "$vol / 5"

        drawPlaces()

        // Earphone pill: green check when connected, ⚡ fast or ⏳ slow.
        val ear = Sunai.earphones.state
        val statusBg = findViewById<View>(R.id.earphone_status_bg)
        val statusIcon = findViewById<ImageView>(R.id.earphone_status_icon)
        val speed = findViewById<ImageView>(R.id.earphone_speed)
        val pill = findViewById<View>(R.id.earphone_pill)
        if (ear.connected) {
            statusBg.background = Ui.circle(this, Ui.color(this, R.color.green))
            statusIcon.setImageResource(R.drawable.ic_check)
            speed.visibility = View.VISIBLE
            speed.setImageResource(if (ear.kind.fast) R.drawable.ic_bolt else R.drawable.ic_hourglass)
            Ui.tint(speed, Ui.color(this, if (ear.kind.fast) R.color.green else R.color.amber))
            pill.background = Ui.round(this, Ui.color(this, R.color.white), 28f, Ui.color(this, R.color.green), 2f)
        } else {
            statusBg.background = Ui.circle(this, Ui.color(this, R.color.amber))
            statusIcon.setImageResource(R.drawable.ic_bluetooth)
            speed.visibility = View.GONE
            pill.background = Ui.round(this, Ui.color(this, R.color.white), 28f, Ui.color(this, R.color.amber), 2f)
        }

        findViewById<View>(R.id.saver_badge).visibility = if (Sunai.saverOn) View.VISIBLE else View.GONE
        drawMeter(if (on) NativeAudio.getLevel() else 0f)
    }

    /**
     * Auto selected: the auto tile is solid blue and the place the AI chose is light blue.
     * Manual: the chosen place is solid blue.
     */
    private fun drawPlaces() {
        val auto = Sunai.prefs.autoPlace
        val current = Sunai.currentPlace
        shownPlace = current
        styleTile(autoTile, if (auto) 2 else 0)
        places.forEachIndexed { i, tile ->
            styleTile(tile, when {
                !auto && i == current -> 2
                auto && i == current && Sunai.listening -> 1
                else -> 0
            })
        }
    }

    /** level 0 = plain, 1 = light highlight (AI's choice), 2 = selected */
    private fun styleTile(tile: LinearLayout, level: Int) {
        val (bg, fg) = when (level) {
            2 -> R.color.blue to R.color.white
            1 -> R.color.blue_soft to R.color.blue
            else -> R.color.white to R.color.ink
        }
        tile.background = Ui.round(this, Ui.color(this, bg), 22f)
        Ui.tint(tile.getChildAt(0) as ImageView, Ui.color(this, fg))
        (tile.getChildAt(1) as TextView).setTextColor(Ui.color(this, fg))
        tile.isSelected = level == 2
    }

    private fun buildMeter() {
        val w = Ui.dp(this, 7f)
        repeat(11) {
            val v = View(this)
            val lp = LinearLayout.LayoutParams(w, Ui.dp(this, 6f))
            lp.marginStart = Ui.dp(this, 2.5f); lp.marginEnd = Ui.dp(this, 2.5f)
            meter.addView(v, lp)
            meterBars += v
        }
    }

    private fun buildVolumeBars() {
        for (i in 1..5) {
            val v = View(this)
            val lp = LinearLayout.LayoutParams(0, Ui.dp(this, 12f + i * 10f), 1f)
            lp.marginStart = Ui.dp(this, 3f); lp.marginEnd = Ui.dp(this, 3f)
            volBars.addView(v, lp)
            volumeBars += v
        }
    }

    private val shape = floatArrayOf(0.3f, 0.5f, 0.75f, 1f, 0.7f, 0.9f, 0.6f, 0.85f, 0.55f, 0.4f, 0.3f)

    private var meterColoredOn: Boolean? = null

    private fun drawMeter(level: Float) {
        val on = Sunai.listening
        if (meterColoredOn != on) {   // only recolour when the state changes
            val color = Ui.color(this, if (on) R.color.green else R.color.line)
            meterBars.forEach { it.background = Ui.round(this, color, 4f) }
            meterColoredOn = on
        }
        meterBars.forEachIndexed { i, bar ->
            val h = Ui.dp(this, if (on) 6f + 28f * shape[i] * level else 6f)
            val lp = bar.layoutParams
            if (lp.height != h) { lp.height = h; bar.layoutParams = lp }
        }
    }

    private val meterTick = object : Runnable {
        override fun run() {
            if (!resumed) return
            if (Sunai.listening) {
                drawMeter(NativeAudio.getLevel())
                // The AI may have switched place: update the highlight.
                if (Sunai.prefs.autoPlace && Sunai.currentPlace != shownPlace) drawPlaces()
            }
            handler.postDelayed(this, 80)
        }
    }

    companion object {
        private const val REQ_MIC = 1
    }
}
