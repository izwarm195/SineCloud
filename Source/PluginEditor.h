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

    // 构造顺序：world 先于 sceneView；析构顺序相反（成员声明顺序决定）
    // sceneView 析构时会调 context.detach()，此时 world 还活着 -> 安全
    std::unique_ptr<sc::World>     world;
    std::unique_ptr<sc::SceneView> sceneView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessorEditor)
};
