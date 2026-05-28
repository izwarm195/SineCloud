/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class SineCloudAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SineCloudAudioProcessorEditor (SineCloudAudioProcessor&);
    ~SineCloudAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SineCloudAudioProcessor& audioProcessor;

    juce::Slider pitchSlider, densitySlider;
    juce::Slider attackSlider, sustainSlider, releaseSlider, decaySlider;
    juce::Label  pitchLabel, densityLabel;
    juce::Label  attackLabel, sustainLabel, releaseLabel, decayLabel;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> pitchAttach, densityAttach;
    std::unique_ptr<SA> attackAttach, sustainAttach, releaseAttach, decayAttach;

};

