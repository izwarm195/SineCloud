#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SineCloudAudioProcessorEditor::SineCloudAudioProcessorEditor(SineCloudAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(640, 360, 1920, 1080);
    setSize(800, 450);
    setWantsKeyboardFocus(false); // 键盘焦点交给 SceneView

    // World 先构造（不依赖 GL，只创建 KnobEntity / Player 对象）
    world = std::make_unique<sc::World>(audioProcessor, lighting);

    // SceneView 后构造，注入 World
    sceneView = std::make_unique<sc::SceneView>();
    sceneView->setWorld(world.get());


    addAndMakeVisible(*sceneView);

    repaint();
    setSize(getWidth() + 1, getHeight());
    setSize(getWidth() - 1, getHeight());//偷偷改一下窗口大小
}

SineCloudAudioProcessorEditor::~SineCloudAudioProcessorEditor()
{
    // 先让 SceneView 脱离 GL context（触发 openGLContextClosing -> releaseMeshes）
    // 再让 world 析构，顺序由成员声明顺序保证（sceneView 先析构，world 后析构）
    // 但为了绝对安全，在析构体里显式断开 World 引用
    if (sceneView != nullptr)
        sceneView->setWorld(nullptr);
}

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
