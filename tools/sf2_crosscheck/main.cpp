/**
    Cross-checks x10::sf2 against FluidSynth on the same bank.

    The reader's unit tests build their own fixtures, which means a
    misunderstanding shared between the fixture builder and the reader is
    invisible to them — they would agree with each other and both be wrong. This
    tool removes that by comparing against an implementation that has been
    reading real SoundFonts for twenty years.

    FluidSynth is LGPL and is used here as an offline development tool only. It
    is never linked into the plugin and never shipped (planning document §10).

    Usage: sf2_crosscheck <bank.sf2> [more.sf2 ...]
    Exit code 0 when every bank agrees, 1 otherwise.
*/

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include <fluidsynth.h>

#include "x10/sf2/Sf2Reader.h"

namespace
{

struct PresetEntry
{
    int         bank = 0;
    int         program = 0;
    std::string name;

    [[nodiscard]] bool operator< (const PresetEntry& other) const
    {
        if (bank != other.bank)       return bank < other.bank;
        if (program != other.program) return program < other.program;
        return name < other.name;
    }

    [[nodiscard]] bool operator== (const PresetEntry& other) const
    {
        return bank == other.bank && program == other.program && name == other.name;
    }
};

/** Trailing spaces are not significant in a SoundFont name field. */
std::string trimmed (std::string text)
{
    while (! text.empty() && (text.back() == ' ' || text.back() == '\0'))
        text.pop_back();
    return text;
}

bool collectFromFluidSynth (const std::string& path, std::vector<PresetEntry>& out)
{
    fluid_settings_t* settings = new_fluid_settings();
    if (settings == nullptr)
        return false;

    // Keep it silent and allocate no audio driver: this is a parser, not a synth.
    fluid_settings_setint (settings, "synth.verbose", 0);

    fluid_synth_t* synth = new_fluid_synth (settings);
    if (synth == nullptr)
    {
        delete_fluid_settings (settings);
        return false;
    }

    bool ok = false;
    const int fontId = fluid_synth_sfload (synth, path.c_str(), 0);

    if (fontId != FLUID_FAILED)
    {
        if (fluid_sfont_t* font = fluid_synth_get_sfont_by_id (synth, fontId))
        {
            fluid_sfont_iteration_start (font);

            while (fluid_preset_t* preset = fluid_sfont_iteration_next (font))
            {
                const char* name = fluid_preset_get_name (preset);
                out.push_back (PresetEntry { fluid_preset_get_banknum (preset),
                                             fluid_preset_get_num (preset),
                                             trimmed (name != nullptr ? name : "") });
            }
            ok = true;
        }
    }

    delete_fluid_synth (synth);
    delete_fluid_settings (settings);
    return ok;
}

bool collectFromX10 (const std::string& path, std::vector<PresetEntry>& out, x10::sf2::Sf2Error& error)
{
    std::vector<std::byte> bytes;
    x10::sf2::RawBank bank;

    error = x10::sf2::readFile (path, bytes, bank);
    if (error != x10::sf2::Sf2Error::ok)
        return false;

    for (const auto& preset : bank.presets)
        out.push_back (PresetEntry { static_cast<int> (preset.bank),
                                     static_cast<int> (preset.preset),
                                     trimmed (preset.name) });
    return true;
}

int compare (const std::string& path)
{
    std::vector<PresetEntry> theirs, ours;

    const bool fluidOk = collectFromFluidSynth (path, theirs);

    x10::sf2::Sf2Error error = x10::sf2::Sf2Error::ok;
    const bool oursOk = collectFromX10 (path, ours, error);

    if (! fluidOk && ! oursOk)
    {
        std::printf ("  both reject      : %s (%s)\n", path.c_str(), x10::sf2::toString (error));
        return 0; // agreeing that a file is bad is agreement
    }

    if (fluidOk != oursOk)
    {
        std::printf ("  DISAGREE on load : %s  fluidsynth=%s  x10=%s (%s)\n",
                     path.c_str(),
                     fluidOk ? "ok" : "reject",
                     oursOk ? "ok" : "reject",
                     x10::sf2::toString (error));
        return 1;
    }

    std::sort (theirs.begin(), theirs.end());
    std::sort (ours.begin(), ours.end());

    if (theirs == ours)
    {
        std::printf ("  match  %3zu preset(s) : %s\n", ours.size(), path.c_str());
        return 0;
    }

    std::printf ("  DISAGREE on presets: %s  fluidsynth=%zu  x10=%zu\n",
                 path.c_str(), theirs.size(), ours.size());

    const std::size_t limit = std::min<std::size_t> (8, std::max (theirs.size(), ours.size()));
    for (std::size_t i = 0; i < limit; ++i)
    {
        const auto describe = [] (const std::vector<PresetEntry>& list, std::size_t index)
        {
            if (index >= list.size())
                return std::string { "-" };
            const auto& e = list[index];
            return std::to_string (e.bank) + ":" + std::to_string (e.program) + " " + e.name;
        };

        std::printf ("    [%zu] fluidsynth=%-32s x10=%s\n",
                     i, describe (theirs, i).c_str(), describe (ours, i).c_str());
    }

    return 1;
}

} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage: sf2_crosscheck <bank.sf2> [more.sf2 ...]\n");
        return 2;
    }

    int disagreements = 0;
    for (int i = 1; i < argc; ++i)
        disagreements += compare (argv[i]);

    std::printf ("\n%d disagreement(s) across %d file(s)\n", disagreements, argc - 1);
    return disagreements == 0 ? 0 : 1;
}
