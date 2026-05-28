#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "IsoSceneDemo.h"


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

    // ---- DREAM ¿ò ----
    juce::Label    dreamGroupLabel;
    InertialSlider dreamSlider, pitchSlider, floatSlider, shimmerSlider, densitySlider, gainSlider;
    juce::Label    dreamLabel, pitchLabel, floatLabel, shimmerLabel, densityLabel, gainLabel;

    // ---- ADSR ¿ò ----
    juce::Label    adsrGroupLabel;
    InertialSlider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label    attackLabel, decayLabel, sustainLabel, releaseLabel;

    // ---- SPACE ¿ò ----
    juce::Label    spaceGroupLabel;
    InertialSlider dlyTimeSlider, dlyFbSlider, dlyMixSlider, revMixSlider, revSizeSlider;
    juce::Label    dlyTimeLabel, dlyFbLabel, dlyMixLabel, revMixLabel, revSizeLabel;

    // ---- Root ÏÔÊ¾ ----
    juce::Label rootDisplay;

    // ---- Attachments ----
    std::unique_ptr<SA> dreamA, pitchA, floatA, shimmerA, densityA, gainA;
    std::unique_ptr<SA> attackA, decayA, sustainA, releaseA;
    std::unique_ptr<SA> dlyTimeA, dlyFbA, dlyMixA, revMixA, revSizeA;

    juce::Rectangle<int> dreamBox, adsrBox, spaceBox;

    void setupKnob(InertialSlider& s, juce::Label& lbl, const juce::String& name,
        const juce::String& suffix, bool valueBox);
    void styleGroupLabel(juce::Label& l, const juce::String& text);

    // ---- demo ----
    std::unique_ptr<IsoSceneDemo> sceneDemo;
    static constexpr bool kUseSceneDemo = true;   // false = 老 UI, true = 场景 demo


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessorEditor)
};
