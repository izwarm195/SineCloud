/*
  ==============================================================================

    SineCloudSound.h
    Created: 28 May 2026 9:41:28am
    Author:  wzm

  ==============================================================================
*/

#pragma once
#pragma once

#include <JuceHeader.h>

class SineCloudSound : public juce::SynthesiserSound
{
public:
    SineCloudSound() {}

    bool appliesToNote(int /*midiNoteNumber*/) override   { return true; }
    bool appliesToChannel(int /*midiChannel*/) override   { return true; }
};
