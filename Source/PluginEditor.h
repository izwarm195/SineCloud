#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SceneView.h"
#include "World.h"

//==============================================================================
class SineCloudAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SineCloudAudioProcessorEditor(SineCloudAudioProcessor&);
    ~SineCloudAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SineCloudAudioProcessor& audioProcessor;

    // 顺序很关键：world 必须在 sceneView 之前构造、之后析构
    std::unique_ptr<sc::World>     world;
    std::unique_ptr<sc::SceneView> sceneView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessorEditor)
};
