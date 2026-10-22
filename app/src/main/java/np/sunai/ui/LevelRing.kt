package np.sunai.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View
import kotlin.math.min

/**
 * Soft rings behind the big ear button that grow with the sound level,
 * so the listener can see the app is hearing something.
 */
class LevelRing @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs) {

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private var shown = 0f
    private var color = 0xFF15803D.toInt()
    private var active = false

    /** Level 0..1; smoothed so the rings move calmly. */
    fun setLevel(level: Float) {
        val target = level.coerceIn(0f, 1f)
        shown += (target - shown) * if (target > shown) 0.6f else 0.25f
        invalidate()
    }

    fun setActive(on: Boolean, ringColor: Int) {
        active = on
        color = ringColor
        if (!on) shown = 0f
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        val cx = width / 2f
        val cy = height / 2f
        val maxR = min(width, height) / 2f
        // The button covers ~78 % of this view; rings live in the space outside it.
        val base = maxR * 0.80f
        val reach = maxR - base
        if (!active) {
            paint.color = color and 0x00FFFFFF or (0x33 shl 24)
            canvas.drawCircle(cx, cy, base + reach * 0.25f, paint)
            return
        }
        // Outer ring: grows with the level. Inner ring: steady halo.
        paint.color = color and 0x00FFFFFF or (0x2E shl 24)
        canvas.drawCircle(cx, cy, base + reach * (0.35f + 0.65f * shown), paint)
        paint.color = color and 0x00FFFFFF or (0x55 shl 24)
        canvas.drawCircle(cx, cy, base + reach * (0.2f + 0.35f * shown), paint)
    }
}
