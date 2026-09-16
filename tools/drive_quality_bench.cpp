// Standalone numeric probe for the Character Drive quality decision.
//
// This intentionally does not depend on JUCE or the plugin target.  It compares
// the existing x10 Tanh curve in three forms:
//   * direct: f(gain * x) / gain
//   * first-order ADAA: the exact interval mean from Adaa1<Tanh>
//   * four midpoint segments: a composite midpoint approximation to that same
//     interval mean (not a separate antialiasing algorithm)
//
// Build from the repository root, for example:
//   c++ -O3 -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
//       -Ilibs/x10_dsp/include tools/drive_quality_bench.cpp \
//       -o /tmp/drive_quality_bench

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <numbers>
#include <string_view>
#include <vector>

#include "x10/dsp/nonlinear/Adaa1.h"
#include "x10/dsp/nonlinear/Curves.h"

namespace
{

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kWindow = 48000;
constexpr double kToneHz = 5000.0;
constexpr std::size_t kToneBin = 5000;
constexpr float kAmplitude = 0.5f;
constexpr float kDriveDb = 12.0f;
constexpr std::size_t kLowLevelWindow = 48000;

[[nodiscard]] float driveGain() noexcept
{
    return static_cast<float> (std::pow (10.0, static_cast<double> (kDriveDb) / 20.0));
}

template <class Curve>
class Direct
{
public:
    explicit Direct (float gain) noexcept : gain_ (gain) {}

    [[nodiscard]] float process (float x) noexcept
    {
        return Curve::f (gain_ * x) / gain_;
    }

    void reset() noexcept {}

private:
    float gain_;
};

template <class Curve>
class Adaa
{
public:
    explicit Adaa (float gain) noexcept : gain_ (gain) {}

    [[nodiscard]] float process (float x) noexcept
    {
        return stage_.process (gain_ * x) / gain_;
    }

    void reset() noexcept { stage_.reset(); }

private:
    float gain_;
    x10::dsp::Adaa1<Curve> stage_;
};

/**
    Latency-preserving ADAA residual:

        y = x[n] + (ADAA(f, x[n]) - (x[n] + x[n-1]) / 2)

    The linear part of the interval average is removed before adding the
    current sample back. Therefore a low-level signal stays at unity gain and
    current-sample phase, while only the nonlinear residual is antialiased.
*/
template <class Curve>
class ResidualAdaa
{
public:
    explicit ResidualAdaa (float gain) noexcept : gain_ (gain) {}

    [[nodiscard]] float process (float x) noexcept
    {
        const float nonlinearMean = stage_.process (gain_ * x) / gain_;
        const float linearMean = 0.5f * (previous_ + x);
        previous_ = x;
        return x + nonlinearMean - linearMean;
    }

    void reset() noexcept
    {
        previous_ = 0.0f;
        stage_.reset();
    }

private:
    float gain_;
    float previous_ = 0.0f;
    x10::dsp::Adaa1<Curve> stage_;
};

/**
    Four midpoint evaluations over [x[n-1], x[n]].

    This is a quadrature approximation to first-order ADAA's exact interval
    mean. It adds four curve calls per sample and keeps the same one-sample
    input history; it is included only to quantify whether that extra cost buys
    anything over the existing exact antiderivative.
*/
template <class Curve>
class Midpoint4
{
public:
    explicit Midpoint4 (float gain) noexcept : gain_ (gain) {}

    [[nodiscard]] float process (float x) noexcept
    {
        const float delta = x - previous_;
        const float x0 = previous_ + 0.125f * delta;
        const float x1 = previous_ + 0.375f * delta;
        const float x2 = previous_ + 0.625f * delta;
        const float x3 = previous_ + 0.875f * delta;

        // Keep the reduction in double: this is a measurement harness, not an
        // audio implementation, and the sum should not add float noise.
        const double sum = static_cast<double> (Curve::f (gain_ * x0))
                         + static_cast<double> (Curve::f (gain_ * x1))
                         + static_cast<double> (Curve::f (gain_ * x2))
                         + static_cast<double> (Curve::f (gain_ * x3));

        previous_ = x;
        return static_cast<float> ((0.25 * sum) / static_cast<double> (gain_));
    }

    void reset() noexcept { previous_ = 0.0f; }

private:
    float gain_;
    float previous_ = 0.0f;
};

[[nodiscard]] std::vector<float> sine (std::size_t count, double frequencyHz, float amplitude)
{
    std::vector<float> result (count);
    const double phaseStep = 2.0 * std::numbers::pi_v<double> * frequencyHz / kSampleRate;

    for (std::size_t n = 0; n < count; ++n)
        result[n] = amplitude * static_cast<float> (std::sin (phaseStep * static_cast<double> (n)));

    return result;
}

[[nodiscard]] double rmsPower (const std::vector<float>& block) noexcept
{
    double sum = 0.0;
    for (const float sample : block)
    {
        const double value = static_cast<double> (sample);
        sum += value * value;
    }

    return sum / static_cast<double> (block.size());
}

[[nodiscard]] double binAmplitude (const std::vector<float>& block, std::size_t bin) noexcept
{
    const double count = static_cast<double> (block.size());
    const double phaseStep = 2.0 * std::numbers::pi_v<double> * static_cast<double> (bin) / count;
    double real = 0.0;
    double imag = 0.0;

    for (std::size_t n = 0; n < block.size(); ++n)
    {
        const double phase = phaseStep * static_cast<double> (n);
        const double sample = static_cast<double> (block[n]);
        real += sample * std::cos (phase);
        imag -= sample * std::sin (phase);
    }

    return 2.0 * std::hypot (real, imag) / count;
}

[[nodiscard]] double decibels (double value) noexcept
{
    return 20.0 * std::log10 (std::max (value, 1.0e-30));
}

template <class Processor>
[[nodiscard]] std::vector<float> renderSteadyState (Processor& processor,
                                                     const std::vector<float>& input)
{
    processor.reset();
    for (const float sample : input)
        (void) processor.process (sample);

    std::vector<float> output (input.size());
    for (std::size_t i = 0; i < input.size(); ++i)
        output[i] = processor.process (input[i]);
    return output;
}

struct Spectrum
{
    double fundamental = 0.0;
    double third = 0.0;
    double fifthFold = 0.0;
    double aliasRms = 0.0;
    double aliasDbc = 0.0;
};

[[nodiscard]] Spectrum measureSpectrum (const std::vector<float>& output)
{
    Spectrum result;
    result.fundamental = binAmplitude (output, kToneBin);
    result.third = binAmplitude (output, 3 * kToneBin);
    // 5 * 5 kHz = 25 kHz, which folds to 48 - 25 = 23 kHz.
    result.fifthFold = binAmplitude (output, 23 * 1000);

    // At a coherent tone all harmonics are on exact DFT bins.  Subtract the
    // DC, 1st..4th harmonic powers from total power; the remainder is the
    // folded fifth-and-higher harmonic energy plus any broadband residue.
    const double totalPower = rmsPower (output);
    const double dc = [&output]
    {
        double sum = 0.0;
        for (const float sample : output)
            sum += static_cast<double> (sample);
        return sum / static_cast<double> (output.size());
    }();

    double accounted = dc * dc;
    for (std::size_t harmonic = 1; harmonic <= 4; ++harmonic)
    {
        const std::size_t bin = harmonic * kToneBin;
        accounted += 0.5 * std::pow (binAmplitude (output, bin), 2.0);
    }

    result.aliasRms = std::sqrt (std::max (totalPower - accounted, 0.0));
    result.aliasDbc = decibels (result.aliasRms) - decibels (result.fundamental);
    return result;
}

template <class Processor>
[[nodiscard]] double benchmarkNsPerSample (Processor& processor,
                                            const std::vector<float>& input,
                                            std::size_t repetitions,
                                            double& checksum)
{
    processor.reset();
    for (const float sample : input)
        (void) processor.process (sample);

    const auto start = std::chrono::steady_clock::now();
    double sum = 0.0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        processor.reset();
        for (const float sample : input)
            sum += static_cast<double> (processor.process (sample));
    }
    const auto finish = std::chrono::steady_clock::now();

    checksum = sum;
    const double elapsedNs = static_cast<double> (
        std::chrono::duration_cast<std::chrono::nanoseconds> (finish - start).count());
    return elapsedNs / static_cast<double> (input.size() * repetitions);
}

template <class Processor>
void printSpectrum (std::string_view name, Processor& processor, const std::vector<float>& input)
{
    const Spectrum spectrum = measureSpectrum (renderSteadyState (processor, input));
    std::printf ("spectrum %-9.*s fundamental_amp %.9f fundamental_dbfs %.3f "
                 "third_amp %.9f third_dbfs %.3f fifth_fold_23k_amp %.9f "
                 "fifth_fold_23k_dbfs %.3f alias_rms_dbfs %.3f alias_dbc %.3f\n",
                 static_cast<int> (name.size()), name.data(), spectrum.fundamental,
                 decibels (spectrum.fundamental), spectrum.third, decibels (spectrum.third),
                 spectrum.fifthFold, decibels (spectrum.fifthFold), decibels (spectrum.aliasRms),
                 spectrum.aliasDbc);
}

template <class Processor>
void printLowLevelGain (std::string_view name, float gain)
{
    std::printf ("low_level %-9.*s", static_cast<int> (name.size()), name.data());
    constexpr std::array<double, 6> frequencies { 1000.0, 5000.0, 10000.0,
                                                   15000.0, 20000.0, 22000.0 };

    for (const double frequency : frequencies)
    {
        const auto input = sine (kLowLevelWindow, frequency, 1.0e-4f);
        Processor processor (gain);
        const auto output = renderSteadyState (processor, input);
        const double ratio = binAmplitude (output,
                                           static_cast<std::size_t> (std::llround (
                                               frequency * static_cast<double> (kLowLevelWindow)
                                               / kSampleRate)))
                           / binAmplitude (input,
                                           static_cast<std::size_t> (std::llround (
                                               frequency * static_cast<double> (kLowLevelWindow)
                                               / kSampleRate)));
        std::printf (" %.0fHz=%.3f", frequency, decibels (ratio));
    }
    std::printf ("\n");
}

} // namespace

int main()
{
    const float gain = driveGain();
    const auto input = sine (kWindow, kToneHz, kAmplitude);

    std::printf ("drive_quality_bench sample_rate=%.0f tone_hz=%.0f amplitude=%.3f drive_db=%.1f gain=%.9f\n",
                 kSampleRate, kToneHz, static_cast<double> (kAmplitude),
                 static_cast<double> (kDriveDb), static_cast<double> (gain));
    std::printf ("note: 23 kHz is the fifth-harmonic fold of 25 kHz; ADAA and midpoint4 "
                 "have one-sample input history.\n");

    Direct<x10::dsp::curves::Tanh> direct (gain);
    Adaa<x10::dsp::curves::Tanh> adaa (gain);
    Midpoint4<x10::dsp::curves::Tanh> midpoint4 (gain);

    printSpectrum ("direct", direct, input);
    printSpectrum ("adaa1", adaa, input);
    ResidualAdaa<x10::dsp::curves::Tanh> residual (gain);
    printSpectrum ("residual", residual, input);
    printSpectrum ("midpoint4", midpoint4, input);

    printLowLevelGain<Direct<x10::dsp::curves::Tanh>> ("direct", gain);
    printLowLevelGain<Adaa<x10::dsp::curves::Tanh>> ("adaa1", gain);
    printLowLevelGain<ResidualAdaa<x10::dsp::curves::Tanh>> ("residual", gain);
    printLowLevelGain<Midpoint4<x10::dsp::curves::Tanh>> ("midpoint4", gain);

    const std::size_t repetitions = 200;
    double directChecksum = 0.0;
    double adaaChecksum = 0.0;
    double residualChecksum = 0.0;
    double midpointChecksum = 0.0;
    const double directNs = benchmarkNsPerSample (direct, input, repetitions, directChecksum);
    const double adaaNs = benchmarkNsPerSample (adaa, input, repetitions, adaaChecksum);
    const double residualNs = benchmarkNsPerSample (residual, input, repetitions, residualChecksum);
    const double midpointNs = benchmarkNsPerSample (midpoint4, input, repetitions, midpointChecksum);

    std::printf ("cpu repetitions=%zu samples_per_variant=%zu\n", repetitions,
                 input.size() * repetitions);
    std::printf ("cpu %-9s ns_per_sample %.3f relative_to_direct %.3fx checksum %.9g\n",
                 "direct", directNs, 1.0, directChecksum);
    std::printf ("cpu %-9s ns_per_sample %.3f relative_to_direct %.3fx checksum %.9g\n",
                 "adaa1", adaaNs, adaaNs / directNs, adaaChecksum);
    std::printf ("cpu %-9s ns_per_sample %.3f relative_to_direct %.3fx checksum %.9g\n",
                 "residual", residualNs, residualNs / directNs, residualChecksum);
    std::printf ("cpu %-9s ns_per_sample %.3f relative_to_direct %.3fx checksum %.9g\n",
                 "midpoint4", midpointNs, midpointNs / directNs, midpointChecksum);

    // Keep the compiler from treating the benchmark's checksums as dead data.
    volatile double benchmarkGuard = directChecksum + adaaChecksum + residualChecksum + midpointChecksum;
    (void) benchmarkGuard;
    return 0;
}
