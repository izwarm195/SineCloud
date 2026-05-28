/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

SineCloudAudioProcessorEditor::SineCloudAudioProcessorEditor(SineCloudAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(600, 350);

    auto setupKnob = [this](juce::Slider& slider, juce::Label& label,
        const juce::String& name, const juce::String& suffix)
        {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
            slider.setTextValueSuffix(suffix);
            addAndMakeVisible(slider);

            label.setText(name, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.attachToComponent(&slider, false);
            addAndMakeVisible(label);
        };

    setupKnob(pitchSlider, pitchLabel, "Pitch", "");
    setupKnob(densitySlider, densityLabel, "Density", " Hz");
    setupKnob(attackSlider, attackLabel, "Attack", " ms");
    setupKnob(sustainSlider, sustainLabel, "Sustain", " ms");
    setupKnob(releaseSlider, releaseLabel, "Release", " ms");
    setupKnob(decaySlider, decayLabel, "Decay", " ms");

    pitchAttach = std::make_unique<SA>(audioProcessor.apvts, SineCloudAudioProcessor::PARAM_PITCH, pitchSlider);
    densityAttach = std::make_unique<SA>(audioProcessor.apvts, SineCloudAudioProcessor::PARAM_DENSITY, densitySlider);
    attackAttach = std::make_unique<SA>(audioProcessor.apvts, SineCloudAudioProcessor::PARAM_ATTACK, attackSlider);
    sustainAttach = std::make_unique<SA>(audioProcessor.apvts, SineCloudAudioProcessor::PARAM_SUSTAIN, sustainSlider);
    releaseAttach = std::make_unique<SA>(audioProcessor.apvts, SineCloudAudioProcessor::PARAM_RELEASE, releaseSlider);
    decayAttach = std::make_unique<SA>(audioProcessor.apvts, SineCloudAudioProcessor::PARAM_DECAY, decaySlider);
}

SineCloudAudioProcessorEditor::~SineCloudAudioProcessorEditor()
{
}

void SineCloudAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("Sine Cloud", getLocalBounds().removeFromTop(40),
        juce::Justification::centred, 1);
}

void SineCloudAudioProcessorEditor::resized()
{
    const int knobSize = 100;
    const int spacing = 20;
    const int row1Y = 70;
    const int row2Y = 210;

    // 第一行：Pitch / Density (居中两个)
    const int row1TotalW = knobSize * 2 + spacing;
    const int row1StartX = (getWidth() - row1TotalW) / 2;
    pitchSlider.setBounds(row1StartX, row1Y, knobSize, knobSize);
    densitySlider.setBounds(row1StartX + knobSize + spacing, row1Y, knobSize, knobSize);

    // 第二行：4 个 ADSR
    const int adsrTotalW = knobSize * 4 + spacing * 3;
    const int adsrStartX = (getWidth() - adsrTotalW) / 2;
    attackSlider.setBounds(adsrStartX, row2Y, knobSize, knobSize);
    sustainSlider.setBounds(adsrStartX + (knobSize + spacing), row2Y, knobSize, knobSize);
    releaseSlider.setBounds(adsrStartX + (knobSize + spacing) * 2, row2Y, knobSize, knobSize);
    decaySlider.setBounds(adsrStartX + (knobSize + spacing) * 3, row2Y, knobSize, knobSize);
}
