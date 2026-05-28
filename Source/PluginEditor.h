#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class SineCloudAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit SineCloudAudioProcessorEditor(SineCloudAudioProcessor&);
    ~SineCloudAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    SineCloudAudioProcessor& audioProcessor;

    // ---- DREAM 框 ----
    juce::Label  dreamGroupLabel;
    juce::Slider dreamSlider, pitchSlider, floatSlider, shimmerSlider, densitySlider, gainSlider;
    juce::Label  dreamLabel, pitchLabel, floatLabel, shimmerLabel, densityLabel, gainLabel;

    // ---- ADSR 框 ----
    juce::Label  adsrGroupLabel;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label  attackLabel, decayLabel, sustainLabel, releaseLabel;

    // ---- SPACE 框 ----
    juce::Label  spaceGroupLabel;
    juce::Slider dlyTimeSlider, dlyFbSlider, dlyMixSlider, revMixSlider, revSizeSlider;
    juce::Label  dlyTimeLabel, dlyFbLabel, dlyMixLabel, revMixLabel, revSizeLabel;

    // ---- Root 显示 ----
    juce::Label rootDisplay;

    // ---- Attachments ----
    std::unique_ptr<SA> dreamA, pitchA, floatA, shimmerA, densityA, gainA;
    std::unique_ptr<SA> attackA, decayA, sustainA, releaseA;
    std::unique_ptr<SA> dlyTimeA, dlyFbA, dlyMixA, revMixA, revSizeA;

    // 三个分组框的几何区域
    juce::Rectangle<int> dreamBox, adsrBox, spaceBox;

    void setupKnob(juce::Slider& s, juce::Label& lbl, const juce::String& name,
        const juce::String& suffix, bool valueBox);
    void styleGroupLabel(juce::Label& l, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessorEditor)
};
