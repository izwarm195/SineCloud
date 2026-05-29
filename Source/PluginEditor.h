#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SceneView.h"

//==============================================================================
// 极简 editor：整个客户区交给一个 GL 场景视口（当前是 MeshTestComponent，
// 模块 2 之后会被替换成 SceneView）。
// 这里不再持有任何 Slider / Label / Attachment。
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

    // 当前阶段：直接挂 MeshTestComponent。
    // 下一步会改成 std::unique_ptr<SceneView> sceneView; 同样的位置。
    std::unique_ptr<sc::SceneView> sceneView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessorEditor)
};
