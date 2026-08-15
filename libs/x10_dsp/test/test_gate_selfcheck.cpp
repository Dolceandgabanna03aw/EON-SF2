#include <catch2/catch_test_macros.hpp>

#include <cstdio>

#include "x10/dsp/nonlinear/Adaa1.h"
#include "x10/dsp/nonlinear/Curves.h"
#include "x10/dsp/nonlinear/Naive.h"

#include "support/Gate.h"
#include "support/Signals.h"

using namespace x10;
using namespace x10::dsp;
using namespace x10::test;

// ---------------------------------------------------------------------------
// The alias gate is the instrument every later numeric claim rests on. An
// instrument that cannot register a failure is worse than no instrument, because
// it produces confident green results. These tests deliberately feed the gate
// known-bad processors and require it to reject them.
//
// This also blocks the specific shortcut of relaxing a threshold until something
// passes: raising minImprovementDb far enough to accept a naive curve makes the
// rejection tests below fail.
// ---------------------------------------------------------------------------

TEST_CASE ("the gate accepts correct ADAA for every curve", "[gate]")
{
    const AliasGate gate;

    SECTION ("Tanh")
    {
        const auto r = gate.evaluate<curves::Tanh> (Adaa1<curves::Tanh>{});
        CAPTURE (r.candidateNmrDb, r.referenceNmrDb, r.improvementDb);
        REQUIRE (r.verdict == Verdict::Accept);
    }

    SECTION ("Tube")
    {
        const auto r = gate.evaluate<curves::Tube> (Adaa1<curves::Tube>{});
        CAPTURE (r.candidateNmrDb, r.referenceNmrDb, r.improvementDb);
        REQUIRE (r.verdict == Verdict::Accept);
    }

    SECTION ("Transformer")
    {
        const auto r = gate.evaluate<curves::Transformer> (Adaa1<curves::Transformer>{});
        CAPTURE (r.candidateNmrDb, r.referenceNmrDb, r.improvementDb);
        REQUIRE (r.verdict == Verdict::Accept);
    }

    SECTION ("Wavefolder")
    {
        const auto r = gate.evaluate<curves::Wavefolder> (Adaa1<curves::Wavefolder>{});
        CAPTURE (r.candidateNmrDb, r.referenceNmrDb, r.improvementDb);
        REQUIRE (r.verdict == Verdict::Accept);
    }
}

TEST_CASE ("the gate rejects a naive substitution for every curve", "[gate][selfcheck]")
{
    // This is the intentional regression the plan calls for, run at library
    // level: swap the antialiased stage for a direct one and confirm the
    // detector fires. Improvement is 0 dB by construction.
    const AliasGate gate;

    SECTION ("Tanh")
    {
        const auto r = gate.evaluate<curves::Tanh> (naive::Direct<curves::Tanh>{});
        CAPTURE (r.improvementDb);
        REQUIRE (r.verdict == Verdict::Reject);
    }

    SECTION ("Tube")
    {
        const auto r = gate.evaluate<curves::Tube> (naive::Direct<curves::Tube>{});
        CAPTURE (r.improvementDb);
        REQUIRE (r.verdict == Verdict::Reject);
    }

    SECTION ("Transformer")
    {
        const auto r = gate.evaluate<curves::Transformer> (naive::Direct<curves::Transformer>{});
        CAPTURE (r.improvementDb);
        REQUIRE (r.verdict == Verdict::Reject);
    }

    SECTION ("Wavefolder")
    {
        const auto r = gate.evaluate<curves::Wavefolder> (naive::Direct<curves::Wavefolder>{});
        CAPTURE (r.improvementDb);
        REQUIRE (r.verdict == Verdict::Reject);
    }
}

TEST_CASE ("the gate rejects naive hard clipping", "[gate][selfcheck]")
{
    // The planning document names hard clipping as the canary for the whole
    // verification loop.
    const AliasGate gate;

    const auto r = gate.evaluate<curves::Tanh> (naive::HardClip{});
    CAPTURE (r.candidateNmrDb, r.referenceNmrDb, r.improvementDb);
    REQUIRE (r.verdict == Verdict::Reject);
}

TEST_CASE ("the gate threshold is not trivially satisfiable", "[gate][selfcheck]")
{
    // Guards the constant itself: 0 dB would accept anything, and a bar above
    // the measured 6.8 dB minimum would reject a correct implementation.
    const AliasGate gate;
    REQUIRE (gate.minImprovementDb >= 3.0);
    REQUIRE (gate.minImprovementDb <= 6.5);
}

TEST_CASE ("emit the numeric baseline", "[gate][baseline]")
{
    // Written next to the test binary. The Python measurement harness planned for
    // Gate 4/5 consumes this shape; the checked-in copy under golden/ is the
    // reference it will diff against.
    const AliasGate gate;

    const auto tanhResult        = gate.evaluate<curves::Tanh> (Adaa1<curves::Tanh>{});
    const auto tubeResult        = gate.evaluate<curves::Tube> (Adaa1<curves::Tube>{});
    const auto transformerResult = gate.evaluate<curves::Transformer> (Adaa1<curves::Transformer>{});
    const auto folderResult      = gate.evaluate<curves::Wavefolder> (Adaa1<curves::Wavefolder>{});

    std::FILE* out = std::fopen ("x10_dsp_baseline.json", "w");
    REQUIRE (out != nullptr);

    std::fprintf (out, "{\n");
    std::fprintf (out, "  \"schema\": 1,\n");
    std::fprintf (out, "  \"sampleRate\": %.1f,\n", kSampleRate);
    std::fprintf (out, "  \"fftSize\": %zu,\n", kFftSize);
    std::fprintf (out, "  \"toneBin\": %zu,\n", gate.toneBin);
    std::fprintf (out, "  \"toneHz\": %.4f,\n", toneFrequencyHz (gate.toneBin));
    std::fprintf (out, "  \"amplitude\": %.2f,\n", static_cast<double> (gate.amplitude));
    std::fprintf (out, "  \"minImprovementDb\": %.2f,\n", gate.minImprovementDb);
    std::fprintf (out, "  \"curves\": {\n");

    auto emit = [out] (const char* name, const GateResult& r, bool last)
    {
        std::fprintf (out,
                      "    \"%s\": { \"adaaNmrDb\": %.3f, \"naiveNmrDb\": %.3f, \"improvementDb\": %.3f }%s\n",
                      name, r.candidateNmrDb, r.referenceNmrDb, r.improvementDb, last ? "" : ",");
    };

    emit ("tanh", tanhResult, false);
    emit ("tube", tubeResult, false);
    emit ("transformer", transformerResult, false);
    emit ("wavefolder", folderResult, true);

    std::fprintf (out, "  }\n}\n");
    std::fclose (out);
}
