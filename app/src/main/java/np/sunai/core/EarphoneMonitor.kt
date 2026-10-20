package np.sunai.core

import android.content.Context
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build
import android.os.Handler
import android.os.Looper

/**
 * Watches which earphone is connected and how fast it is.
 * Fast = wired, USB-C, hearing-aid (ASHA) and LE Audio. Slow = classic Bluetooth (A2DP/SCO).
 */
class EarphoneMonitor(context: Context) {

    enum class Kind(val fast: Boolean) {
        WIRED(true), USB(true), HEARING_AID(true), LE_AUDIO(true),
        BLUETOOTH_CLASSIC(false), NONE(false)
    }

    data class State(val kind: Kind, val outputId: Int, val micId: Int) {
        val connected get() = kind != Kind.NONE
    }

    private val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val listeners = mutableListOf<(State) -> Unit>()

    var state: State = compute()
        private set

    private val callback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(added: Array<out AudioDeviceInfo>) = refresh()
        override fun onAudioDevicesRemoved(removed: Array<out AudioDeviceInfo>) = refresh()
    }

    init {
        am.registerAudioDeviceCallback(callback, Handler(Looper.getMainLooper()))
    }

    fun addListener(l: (State) -> Unit) { listeners += l }
    fun removeListener(l: (State) -> Unit) { listeners -= l }

    private fun refresh() {
        val new = compute()
        if (new != state) {
            state = new
            listeners.toList().forEach { it(new) }
        }
    }

    private fun compute(): State {
        val outs = am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
        // Prefer the fastest connection when several are present.
        val ranked = outs.mapNotNull { d -> kindOf(d.type)?.let { it to d } }
            .sortedBy { (k, _) -> k.ordinal }
        val best = ranked.firstOrNull()
        // Always listen with the phone's own microphone (earbud mics are too close
        // to the speaker and would squeal).
        val mic = am.getDevices(AudioManager.GET_DEVICES_INPUTS)
            .firstOrNull { it.type == AudioDeviceInfo.TYPE_BUILTIN_MIC }
        return State(best?.first ?: Kind.NONE, best?.second?.id ?: 0, mic?.id ?: 0)
    }

    private fun kindOf(type: Int): Kind? = when {
        type == AudioDeviceInfo.TYPE_WIRED_HEADSET ||
            type == AudioDeviceInfo.TYPE_WIRED_HEADPHONES -> Kind.WIRED
        type == AudioDeviceInfo.TYPE_USB_HEADSET -> Kind.USB
        Build.VERSION.SDK_INT >= 28 && type == AudioDeviceInfo.TYPE_HEARING_AID -> Kind.HEARING_AID
        Build.VERSION.SDK_INT >= 31 && (type == AudioDeviceInfo.TYPE_BLE_HEADSET ||
            type == AudioDeviceInfo.TYPE_BLE_SPEAKER) -> Kind.LE_AUDIO
        type == AudioDeviceInfo.TYPE_BLUETOOTH_A2DP ||
            type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO -> Kind.BLUETOOTH_CLASSIC
        else -> null   // phone speaker, earpiece etc. are never used for listening
    }
}
