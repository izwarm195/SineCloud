#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SceneView.h"
#include "World.h"
#include "Lighting.h"   // [!code ++]

class SineCloudAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SineCloudAudioProcessorEditor(SineCloudAudioProcessor&);
    ~SineCloudAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SineCloudAudioProcessor& audioProcessor;

    // 声明顺序很重要：lighting 先于 world，world 先于 sceneView
    sc::Lighting lighting;                       // [!code ++]
    std::unique_ptr<sc::World>     world;
    std::unique_ptr<sc::SceneView> sceneView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessorEditor)
};
