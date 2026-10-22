package np.sunai.ui

import android.content.Context
import android.content.res.ColorStateList
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView

/** Small drawing helpers so every screen uses the same shapes and colours. */
object Ui {
    fun dp(context: Context, v: Float): Int = (v * context.resources.displayMetrics.density + 0.5f).toInt()

    fun color(context: Context, res: Int): Int = context.getColor(res)

    /** True only in debug builds (used for automated screenshots). */
    fun isDebug(context: Context): Boolean =
        (context.applicationInfo.flags and android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE) != 0

    fun circle(context: Context, fill: Int, ring: Int = 0, ringDp: Float = 0f) = GradientDrawable().apply {
        shape = GradientDrawable.OVAL
        setColor(fill)
        if (ringDp > 0f) setStroke(dp(context, ringDp), ring)
    }

    fun round(context: Context, fill: Int, radiusDp: Float, stroke: Int = 0, strokeDp: Float = 0f) =
        GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = dp(context, radiusDp).toFloat()
            setColor(fill)
            if (strokeDp > 0f) setStroke(dp(context, strokeDp), stroke)
        }

    fun tint(image: ImageView, color: Int) {
        image.imageTintList = ColorStateList.valueOf(color)
    }

    /** A pill-shaped on/off switch built from a FrameLayout track and a knob child. */
    fun setToggle(track: FrameLayout, knob: View, on: Boolean) {
        val c = track.context
        track.background = round(c, color(c, if (on) np.sunai.R.color.green else np.sunai.R.color.toggle_off), 20f)
        val lp = knob.layoutParams as FrameLayout.LayoutParams
        lp.gravity = Gravity.CENTER_VERTICAL or (if (on) Gravity.END else Gravity.START)
        knob.layoutParams = lp
        knob.background = circle(c, color(c, np.sunai.R.color.white))
        track.isSelected = on
    }
}
