#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SceneView.h"
#include "World.h"
#include "Lighting.h"

class SineCloudAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SineCloudAudioProcessorEditor(SineCloudAudioProcessor&);
    ~SineCloudAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SineCloudAudioProcessor& audioProcessor;
    // ★ 删掉 sc::Lighting lighting; —— SceneView 自带的 lighting 是唯一实例

    std::unique_ptr<sc::World> world;
    std::unique_ptr<sc::SceneView> sceneView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessorEditor)
};
