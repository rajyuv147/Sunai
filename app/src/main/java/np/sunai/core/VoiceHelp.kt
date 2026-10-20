package np.sunai.core

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.os.Handler
import android.os.Looper
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import np.sunai.R
import java.util.Locale

/**
 * Spoken help for people who cannot read.
 *
 * 1. If a recorded clip exists at res/raw/voice_<lang>_<key>.ogg it is played
 *    (best: a warm, local voice; ~10-20 KB per clip).
 * 2. Otherwise the phone's offline text-to-speech reads the Nepali text.
 */
class VoiceHelp(private val context: Context, private val prefs: Prefs) {

    enum class Key(val textRes: Int) {
        HOME(R.string.say_home),
        CONNECT(R.string.say_connect),
        TEST(R.string.say_test),
        HELPER(R.string.say_helper),
        NO_EARPHONE(R.string.say_no_earphone),
        CONNECTED(R.string.say_connected),
        SAVER_ON(R.string.say_saver_on),
        LOUD(R.string.say_loud),
        TEST_DONE(R.string.say_test_done),
    }

    private var player: MediaPlayer? = null
    private var tts: TextToSpeech? = null
    private var ttsReady = false
    private val mainHandler = Handler(Looper.getMainLooper())
    var onSpeakingChanged: ((Boolean) -> Unit)? = null

    val speaking: Boolean get() = player?.isPlaying == true || tts?.isSpeaking == true

    fun say(key: Key) {
        stop()
        val name = "voice_${prefs.voiceLang}_${key.name.lowercase(Locale.ROOT)}"
        @Suppress("DiscouragedApi")
        val resId = context.resources.getIdentifier(name, "raw", context.packageName)
        if (resId != 0) playClip(resId) else speakText(context.getString(key.textRes))
    }

    fun stop() {
        player?.run { if (isPlaying) stop(); release() }
        player = null
        tts?.stop()
        onSpeakingChanged?.invoke(false)
    }

    private fun playClip(resId: Int) {
        val mp = MediaPlayer.create(context, resId) ?: return
        mp.setAudioAttributes(
            AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build()
        )
        mp.setOnCompletionListener { stop() }
        player = mp
        mp.start()
        onSpeakingChanged?.invoke(true)
    }

    private fun speakText(text: String) {
        val engine = tts
        if (engine == null) {
            tts = TextToSpeech(context) { status ->
                ttsReady = status == TextToSpeech.SUCCESS
                if (ttsReady) {
                    tts?.language = Locale("ne", "NP")
                    tts?.setSpeechRate(0.85f)   // a little slower for older listeners
                    tts?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
                        override fun onStart(utteranceId: String?) {}
                        override fun onDone(utteranceId: String?) { mainHandler.post { onSpeakingChanged?.invoke(false) } }
                        @Deprecated("Deprecated in Java")
                        override fun onError(utteranceId: String?) { mainHandler.post { onSpeakingChanged?.invoke(false) } }
                    })
                    speakText(text)
                }
            }
            return
        }
        if (!ttsReady) return
        engine.speak(text, TextToSpeech.QUEUE_FLUSH, null, "sunai")
        onSpeakingChanged?.invoke(true)
    }

    fun release() {
        stop()
        tts?.shutdown()
        tts = null
    }
}
