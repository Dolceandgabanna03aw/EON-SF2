#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "x10/dsp/filter/DCBlocker.h"
#include "x10/dsp/filter/TptSvf.h"

#include "support/Signals.h"
#include "support/Spectrum.h"

using namespace x10;
using namespace x10::dsp;
using namespace x10::test;

namespace
{

/** RMS of the steady-state response, discarding the first window. */
template <class P>
double steadyStateRms (P& processor, std::size_t bin, float amplitude)
{
    processor.reset();

    const auto warmUp = binCentredSine (kFftSize, bin, amplitude);
    for (float s : warmUp)
        (void) processor.process (s);

    auto block = binCentredSine (kFftSize, bin, amplitude);
    double sum = 0.0;
    for (float s : block)
    {
        const double y = static_cast<double> (processor.process (s));
        sum += y * y;
    }

    return std::sqrt (sum / static_cast<double> (block.size()));
}

double magnitudeDb (double outputRms, float amplitude)
{
    const double inputRms = static_cast<double> (amplitude) / std::sqrt (2.0);
    return 20.0 * std::log10 (outputRms / inputRms);
}

} // namespace

TEST_CASE ("DCBlocker removes a constant offset", "[filter]")
{
    DCBlocker blocker;
    blocker.prepare (kSampleRate, 20.0f);

    float last = 0.0f;
    for (int i = 0; i < 48000; ++i)
        last = blocker.process (1.0f);

    CAPTURE (last);
    REQUIRE (std::abs (last) < 1.0e-3f);
}

TEST_CASE ("DCBlocker passes audio-band content essentially untouched", "[filter]")
{
    DCBlocker blocker;
    blocker.prepare (kSampleRate, 20.0f);

    const double rms = steadyStateRms (blocker, kGateToneBin, 1.0f);
    const double gainDb = magnitudeDb (rms, 1.0f);

    CAPTURE (gainDb);
    REQUIRE (gainDb > -0.1);
    REQUIRE (gainDb < 0.1);
}

TEST_CASE ("DCBlocker attenuates well below its corner", "[filter]")
{
    DCBlocker blocker;
    blocker.prepare (kSampleRate, 20.0f);

    // Bin 1 is ~2.93 Hz, nearly three octaves below the 20 Hz corner.
    const double rms = steadyStateRms (blocker, 1, 1.0f);
    const double gainDb = magnitudeDb (rms, 1.0f);

    CAPTURE (gainDb);
    REQUIRE (gainDb < -12.0);
}

TEST_CASE ("TptSvf is 3 dB down at its cutoff with Butterworth Q", "[filter]")
{
    TptSvf filter;
    filter.prepare (kSampleRate);
    filter.setCutoff (static_cast<float> (toneFrequencyHz (kGateToneBin)), 0.7071068f);

    const double gainDb = magnitudeDb (steadyStateRms (filter, kGateToneBin, 1.0f), 1.0f);

    CAPTURE (gainDb);
    REQUIRE (gainDb > -3.5);
    REQUIRE (gainDb < -2.5);
}

TEST_CASE ("TptSvf rolls off at twelve dB per octave", "[filter]")
{
    TptSvf filter;
    filter.prepare (kSampleRate);
    filter.setCutoff (500.0f, 0.7071068f);

    // Bins 683 (~2 kHz, two octaves up) and 1365 (~4 kHz, three octaves up).
    const double twoOctaves   = magnitudeDb (steadyStateRms (filter, 683, 1.0f), 1.0f);
    const double threeOctaves = magnitudeDb (steadyStateRms (filter, 1365, 1.0f), 1.0f);

    CAPTURE (twoOctaves, threeOctaves);
    REQUIRE (threeOctaves < twoOctaves);
    REQUIRE (std::abs ((threeOctaves - twoOctaves) - (-12.0)) < 2.5);
}

TEST_CASE ("TptSvf resonance lifts the response at cutoff", "[filter]")
{
    TptSvf filter;
    filter.prepare (kSampleRate);
    filter.setCutoff (static_cast<float> (toneFrequencyHz (kGateToneBin)), 8.0f);

    const double gainDb = magnitudeDb (steadyStateRms (filter, kGateToneBin, 1.0f), 1.0f);

    // |H(fc)| = Q for a state variable lowpass.
    CAPTURE (gainDb);
    REQUIRE (gainDb > 16.0);
    REQUIRE (gainDb < 20.0);
}

TEST_CASE ("TptSvf stays finite at extreme settings", "[filter]")
{
    // Cutoff requests above Nyquist are the classic way to make tan() blow up.
    for (float cutoff : { 1.0f, 10.0f, 20000.0f, 24000.0f, 96000.0f })
    {
        TptSvf filter;
        filter.prepare (kSampleRate);
        filter.setCutoff (cutoff, 20.0f);

        const auto input = binCentredSine (8192, 4097, 4.0f);
        for (float s : input)
        {
            const auto out = filter.processAll (s);
            CAPTURE (cutoff, s);
            REQUIRE (std::isfinite (out.lowpass));
            REQUIRE (std::isfinite (out.bandpass));
            REQUIRE (std::isfinite (out.highpass));
        }
    }
}
