// Second-order IIR filter (RBJ "Audio EQ Cookbook" designs).
// Time-domain and sample-by-sample, so it adds no buffering delay.
#pragma once

#include <cmath>

namespace sunai {

class Biquad {
public:
    void setHighPass(float fs, float freq, float q = 0.7071f) {
        float w0 = 2.0f * kPi * freq / fs;
        float cosw = std::cos(w0), alpha = std::sin(w0) / (2.0f * q);
        float a0 = 1.0f + alpha;
        set((1.0f + cosw) * 0.5f / a0, -(1.0f + cosw) / a0, (1.0f + cosw) * 0.5f / a0,
            -2.0f * cosw / a0, (1.0f - alpha) / a0);
    }

    // Bell-shaped boost or cut around `freq`.
    void setPeaking(float fs, float freq, float gainDb, float q = 1.0f) {
        if (std::fabs(gainDb) < 0.01f) { setBypass(); return; }
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * kPi * freq / fs;
        float cosw = std::cos(w0), alpha = std::sin(w0) / (2.0f * q);
        float a0 = 1.0f + alpha / A;
        set((1.0f + alpha * A) / a0, (-2.0f * cosw) / a0, (1.0f - alpha * A) / a0,
            (-2.0f * cosw) / a0, (1.0f - alpha / A) / a0);
    }

    // Boost or cut everything above `freq` (used for the 8 kHz band).
    void setHighShelf(float fs, float freq, float gainDb, float slope = 1.0f) {
        if (std::fabs(gainDb) < 0.01f) { setBypass(); return; }
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w0 = 2.0f * kPi * freq / fs;
        float cosw = std::cos(w0), sinw = std::sin(w0);
        float alpha = sinw / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        float sqA = 2.0f * std::sqrt(A) * alpha;
        float a0 = (A + 1.0f) - (A - 1.0f) * cosw + sqA;
        set(A * ((A + 1.0f) + (A - 1.0f) * cosw + sqA) / a0,
            -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw) / a0,
            A * ((A + 1.0f) + (A - 1.0f) * cosw - sqA) / a0,
            2.0f * ((A - 1.0f) - (A + 1.0f) * cosw) / a0,
            ((A + 1.0f) - (A - 1.0f) * cosw - sqA) / a0);
    }

    void setBypass() { set(1.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
    bool isBypass() const { return mBypass; }
    void reset() { mZ1 = mZ2 = 0.0f; }

    inline float process(float x) {
        if (mBypass) return x;
        float y = mB0 * x + mZ1;
        mZ1 = mB1 * x - mA1 * y + mZ2;
        mZ2 = mB2 * x - mA2 * y;
        // Flush tiny values so old CPUs don't slow down on denormal numbers.
        if (std::fabs(mZ1) < 1e-20f) mZ1 = 0.0f;
        if (std::fabs(mZ2) < 1e-20f) mZ2 = 0.0f;
        return y;
    }

private:
    static constexpr float kPi = 3.14159265358979f;

    void set(float b0, float b1, float b2, float a1, float a2) {
        mB0 = b0; mB1 = b1; mB2 = b2; mA1 = a1; mA2 = a2;
        mBypass = (b0 == 1.0f && b1 == 0.0f && b2 == 0.0f && a1 == 0.0f && a2 == 0.0f);
    }

    float mB0 = 1.0f, mB1 = 0.0f, mB2 = 0.0f, mA1 = 0.0f, mA2 = 0.0f;
    float mZ1 = 0.0f, mZ2 = 0.0f;
    bool mBypass = true;
};

}  // namespace sunai
