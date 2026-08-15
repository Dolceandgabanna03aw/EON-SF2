#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include "x10/dsp/Config.h"

namespace x10::dsp
{

/**
    Topology-preserving-transform state variable filter (Zavalishin / Simper).

    Chosen over a biquad because the cutoff can be modulated per sample without
    the coefficient-interpolation artefacts a direct-form biquad produces, which
    matters once the SF2 modulation envelope drives initialFilterFc.

    process() returns the lowpass output, matching the SF2 voice topology.
    processAll() exposes all three outputs for the bus stage.
*/
class TptSvf
{
public:
    struct Outputs
    {
        float lowpass;
        float bandpass;
        float highpass;
    };

    void prepare (double sampleRate) noexcept
    {
        sampleRate_ = sampleRate;
        updateCoefficients();
        reset();
    }

    void setCutoff (float cutoffHz, float q) noexcept
    {
        cutoffHz_ = cutoffHz;
        q_        = std::max (q, 0.05f);
        updateCoefficients();
    }

    [[nodiscard]] float process (float x) noexcept { return processAll (x).lowpass; }

    [[nodiscard]] Outputs processAll (float x) noexcept
    {
        const float v3 = x - s2_;
        const float v1 = a1_ * s1_ + a2_ * v3;
        const float v2 = s2_ + a2_ * s1_ + a3_ * v3;

        s1_ = 2.0f * v1 - s1_;
        s2_ = 2.0f * v2 - s2_;

        return { v2, v1, x - k_ * v1 - v2 };
    }

    void reset() noexcept
    {
        s1_ = 0.0f;
        s2_ = 0.0f;
    }

private:
    void updateCoefficients() noexcept
    {
        // Keep the prewarped cutoff clear of Nyquist; tan() diverges there.
        const double limit = sampleRate_ * 0.49;
        const double fc    = std::clamp (static_cast<double> (cutoffHz_), 10.0, limit);
        const double g     = std::tan (std::numbers::pi_v<double> * fc / sampleRate_);

        k_  = 1.0f / q_;
        a1_ = static_cast<float> (1.0 / (1.0 + g * (g + static_cast<double> (k_))));
        a2_ = static_cast<float> (g) * a1_;
        a3_ = static_cast<float> (g) * a2_;
    }

    double sampleRate_ = kReferenceSampleRate;
    float cutoffHz_    = 1000.0f;
    float q_           = 0.7071068f;

    float k_  = 1.4142136f;
    float a1_ = 0.0f;
    float a2_ = 0.0f;
    float a3_ = 0.0f;

    float s1_ = 0.0f;
    float s2_ = 0.0f;
};

} // namespace x10::dsp
