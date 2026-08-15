#pragma once

#include "x10/dsp/nonlinear/Naive.h"

#include "support/Signals.h"
#include "support/Spectrum.h"

namespace x10::test
{

enum class Verdict
{
    Accept,
    Reject
};

struct GateResult
{
    double candidateNmrDb = 0.0;
    double referenceNmrDb = 0.0;
    double improvementDb  = 0.0;
    Verdict verdict       = Verdict::Reject;
};

/**
    Alias-suppression gate.

    The gate is deliberately RELATIVE — it compares a candidate against the
    naive evaluation of the same curve — because an absolute NMR threshold is
    unsound across curves. Measured at the gate operating point, correct ADAA
    wavefolding scores -9.8 dB while a naive hard clipper scores much better in
    absolute terms simply because a triangle folder is harmonically richer than
    a clipper. Any single absolute threshold therefore either accepts a naive
    implementation or rejects a correct one.

    Comparing like with like removes that confound: the only thing the ratio can
    measure is whether the antialiasing machinery is doing anything.

    The threshold is pinned to measurement, not to the planning document. See
    PROVENANCE.md: first-order ADAA delivers 6.8-8.6 dB here, so the bar sits at
    5 dB — comfortably under every measured value and far above the 0 dB a naive
    substitution scores.
*/
struct AliasGate
{
    std::size_t toneBin      = kGateToneBin;
    float amplitude          = kGateAmplitude;
    double minImprovementDb  = 5.0;

    template <dsp::Curve C, class P>
    [[nodiscard]] GateResult evaluate (P candidate) const
    {
        dsp::naive::Direct<C> reference;

        const auto ref  = measureAlias (reference, toneBin, amplitude);
        const auto cand = measureAlias (candidate, toneBin, amplitude);

        GateResult result;
        result.referenceNmrDb = ref.nmrDb();
        result.candidateNmrDb = cand.nmrDb();
        result.improvementDb  = result.referenceNmrDb - result.candidateNmrDb;
        result.verdict        = (result.improvementDb >= minImprovementDb) ? Verdict::Accept
                                                                          : Verdict::Reject;
        return result;
    }
};

} // namespace x10::test
