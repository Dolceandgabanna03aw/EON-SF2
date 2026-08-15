#pragma once

#include <cmath>
#include <numbers>

#include "x10/dsp/Config.h"

namespace x10::dsp
{

/**
    One-pole DC blocker: y[n] = x[n] - x[n-1] + R y[n-1].

    Required after any asymmetric curve. Tube and Transformer rectify the signal
    slightly, and the resulting offset would otherwise accumulate into the voice
    mixer and eat headroom before the bus stage even sees it.
*/
class DCBlocker
{
public:
    void prepare (double sampleRate, float cutoffHz = 20.0f) noexcept
    {
        const double w = 2.0 * std::numbers::pi_v<double> * static_cast<double> (cutoffHz) / sampleRate;
        r_ = static_cast<float> (std::exp (-w));
        reset();
    }

    [[nodiscard]] float process (float x) noexcept
    {
        const float y = x - x1_ + r_ * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }

    void reset() noexcept
    {
        x1_ = 0.0f;
        y1_ = 0.0f;
    }

private:
    float r_  = 0.9973932f; // 20 Hz at 48 kHz
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};

} // namespace x10::dsp
