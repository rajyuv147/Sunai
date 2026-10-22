package np.sunai.ui

import android.Manifest
import android.content.Intent
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
    private lateinit var ring: LevelRing
    private lateinit var volBars: LinearLayout
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
        ring = findViewById(R.id.level_ring)
        volBars = findViewById(R.id.vol_bars)

        buildVolumeBars()
        setupVoiceButton()
        setupNav(0)

        bigButton.setOnClickListener { toggleListening() }
        findViewById<View>(R.id.vol_down).setOnClickListener { Sunai.setVolume(Sunai.prefs.volume - 1) }
        findViewById<View>(R.id.vol_up).setOnClickListener { Sunai.setVolume(Sunai.prefs.volume + 1) }
        places.forEachIndexed { i, tile -> tile.setOnClickListener { Sunai.pickPlace(i) } }
        autoTile.setOnClickListener { Sunai.setAutoPlace(true) }
        findViewById<View>(R.id.earphone_pill).setOnClickListener { goTo(ConnectActivity::class.java) }
        openForScreenshot()

        // Static backgrounds
        findViewById<View>(R.id.vol_down).background =
            Ui.circle(this, Ui.color(this, R.color.blue_soft), Ui.color(this, R.color.blue), 3f)
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
            Ui.circle(this, Ui.color(this, R.color.green), Ui.color(this, R.color.white), 6f)
        else
            Ui.circle(this, Ui.color(this, R.color.white), Ui.color(this, R.color.red), 6f)
        ring.setActive(on, Ui.color(this, if (on) R.color.green else R.color.red))
        if (on) {
            bigIcon.setImageResource(R.drawable.ic_ear_on)
            Ui.tint(bigIcon, Ui.color(this, R.color.white))
        } else {
            bigIcon.setImageResource(R.drawable.ic_ear_off)   // two-colour icon, not tinted
            bigIcon.imageTintList = null
        }
        bigButton.contentDescription = getString(if (on) R.string.a11y_turn_off else R.string.a11y_turn_on)
        // ON/OFF as a solid coloured pill: readable from a distance and in sunlight.
        stateWord.setText(if (on) R.string.state_on else R.string.state_off)
        stateWord.background = Ui.round(this, Ui.color(this, if (on) R.color.green else R.color.red), 24f)

        // Volume bars
        val vol = Sunai.prefs.volume
        volumeBars.forEachIndexed { i, bar ->
            bar.background = if (i < vol) Ui.round(this, Ui.color(this, R.color.blue), 8f)
                             else Ui.round(this, Ui.color(this, R.color.tile), 8f, Ui.color(this, R.color.card_line), 1.5f)
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
            pill.background = Ui.round(this, Ui.color(this, R.color.white), 28f, Ui.color(this, R.color.green), 3f)
        } else {
            statusBg.background = Ui.circle(this, Ui.color(this, R.color.amber))
            statusIcon.setImageResource(R.drawable.ic_bluetooth)
            speed.visibility = View.GONE
            pill.background = Ui.round(this, Ui.color(this, R.color.white), 28f, Ui.color(this, R.color.amber), 3f)
        }

        findViewById<View>(R.id.saver_badge).visibility = if (Sunai.saverOn) View.VISIBLE else View.GONE
        if (on) ring.setLevel(NativeAudio.getLevel())
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

    /** level 0 = plain, 1 = outlined (the AI's choice), 2 = selected (solid blue) */
    private fun styleTile(tile: LinearLayout, level: Int) {
        val fg = when (level) { 2 -> R.color.white; 1 -> R.color.blue; else -> R.color.ink }
        tile.background = when (level) {
            2 -> Ui.round(this, Ui.color(this, R.color.blue), 18f)
            1 -> Ui.round(this, Ui.color(this, R.color.blue_soft), 18f, Ui.color(this, R.color.blue), 3f)
            else -> Ui.round(this, Ui.color(this, R.color.tile), 18f)
        }
        Ui.tint(tile.getChildAt(0) as ImageView, Ui.color(this, fg))
        (tile.getChildAt(1) as TextView).setTextColor(Ui.color(this, fg))
        tile.isSelected = level == 2
    }

    private fun buildVolumeBars() {
        // A staircase: each step taller, like loudness growing.
        for (i in 1..5) {
            val v = View(this)
            val lp = LinearLayout.LayoutParams(0, Ui.dp(this, 16f + i * 8.8f), 1f)
            lp.marginStart = Ui.dp(this, 3f); lp.marginEnd = Ui.dp(this, 3f)
            volBars.addView(v, lp)
            volumeBars += v
        }
    }

    private val meterTick = object : Runnable {
        override fun run() {
            if (!resumed) return
            if (Sunai.listening) {
                ring.setLevel(NativeAudio.getLevel())
                // The AI may have switched place: update the highlight.
                if (Sunai.prefs.autoPlace && Sunai.currentPlace != shownPlace) drawPlaces()
            }
            handler.postDelayed(this, 60)
        }
    }

    /** Debug builds only: `adb shell am start ... --es open helper` opens a screen for screenshots. */
    private fun openForScreenshot() {
        if (!Ui.isDebug(this)) return
        val target = when (intent.getStringExtra("open")) {
            "connect" -> ConnectActivity::class.java
            "test" -> TestActivity::class.java
            "helper" -> HelperActivity::class.java
            else -> return
        }
        startActivity(Intent(this, target).putExtras(intent))
    }

    companion object {
        private const val REQ_MIC = 1
    }
}
