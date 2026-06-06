#include "PluginProcessor.h"
#include "PluginEditor.h"

SineCloudAudioProcessorEditor::SineCloudAudioProcessorEditor(SineCloudAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(640, 360, 1920, 1080);
    setSize(800, 450);
    setWantsKeyboardFocus(false);

    // ★ 先建 SceneView（因为 World 需要它的 lighting 引用）
    sceneView = std::make_unique<sc::SceneView>();

    // ★ World 引用 SceneView 里唯一的 Lighting 实例
    world = std::make_unique<sc::World>(audioProcessor, sceneView->getLighting());

    sceneView->setWorld(world.get());
    addAndMakeVisible(*sceneView);

    repaint();
    setSize(getWidth() + 1, getHeight());
    setSize(getWidth() - 1, getHeight());
}

SineCloudAudioProcessorEditor::~SineCloudAudioProcessorEditor()
{
    if (sceneView != nullptr)
        sceneView->setWorld(nullptr);
}
// ... paint / resized 不变


//==============================================================================
void SineCloudAudioProcessorEditor::paint(juce::Graphics& g)
{
    // GL 铺满全屏；这里只画 context 还没接管前的一帧背景
    g.fillAll(juce::Colours::black);
}

void SineCloudAudioProcessorEditor::resized()
{
    if (sceneView != nullptr)
        sceneView->setBounds(getLocalBounds());
}
