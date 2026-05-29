#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SineCloudAudioProcessorEditor::SineCloudAudioProcessorEditor(SineCloudAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(640, 360, 1920, 1080);
    setSize(960, 600);

    sceneView = std::make_unique<sc::SceneView>();
    addAndMakeVisible(*sceneView);


    setWantsKeyboardFocus(true);
}

SineCloudAudioProcessorEditor::~SineCloudAudioProcessorEditor() = default;

//==============================================================================
void SineCloudAudioProcessorEditor::paint(juce::Graphics& g)
{
    // GL 场景会铺满全屏；这里只画 GL context 还没接管前的一帧背景。
    g.fillAll(juce::Colours::black);
}

void SineCloudAudioProcessorEditor::resized()
{
    if (sceneView != nullptr)
        sceneView->setBounds(getLocalBounds());
}
