#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstdlib>
#include <iostream>

/**
    Headless UI snapshot tool.

    Creates the real RomplerEditor without a host window, gives it a few timer
    ticks so the peak meter and any async state settle, then renders the
    component into an Image and writes it as a PNG.

    Usage:
        ui_shot <output.png>
*/
int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: ui_shot <output.png>\n";
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    aod::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::cerr << "createEditor returned null\n";
        return 1;
    }

    // Let the editor's Timer run a handful of ticks so meter state / labels
    // have a chance to draw in their settled form.
    for (int i = 0; i < 8; ++i)
        juce::Thread::sleep (50);

    const auto bounds = editor->getBounds();
    const int w = juce::jmax (1, bounds.getWidth());
    const int h = juce::jmax (1, bounds.getHeight());

    juce::Image snapshot (juce::Image::ARGB, w, h, true);
    juce::Graphics g (snapshot);
    g.setColour (juce::Colours::black);
    g.fillAll();
    editor->paintEntireComponent (g, false);

    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    juce::PNGImageFormat png;
    juce::FileOutputStream stream (outFile);
    if (! stream.openedOk())
    {
        std::cerr << "cannot open output: " << outFile.getFullPathName() << "\n";
        return 1;
    }

    if (! png.writeImageToStream (snapshot, stream))
    {
        std::cerr << "PNG write failed\n";
        return 1;
    }

    std::cout << "wrote " << outFile.getFullPathName()
              << " (" << w << "x" << h << ")\n";
    return 0;
}
