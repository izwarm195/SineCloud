/*
  ==============================================================================
    InputState.h
    Layer 3: Game / Interaction
    一帧输入快照。由 SceneView 在每次 World::update 前填好，World 内的 Entity
    只读。鼠标按下/抬起以"事件"形式存在（justPressed / justReleased），位置始
    终可用，便于做拾取与拖拽。
  ==============================================================================*/
#pragma once

#include <JuceHeader.h>

namespace sc
{
    struct InputState
    {
        // ---- 键盘（持续按下） ----
        bool keyUp{ false };
        bool keyDown{ false };
        bool keyLeft{ false };
        bool keyRight{ false };
        bool keyAttack{ false };   // 暂时占位，日后给 BossEntity 用

        // ---- 鼠标 ----
        juce::Point<float> mousePos{ 0.0f, 0.0f };
        bool mouseDown{ false }; // 左键当前是否按下
        bool mouseJustPressed{ false }; // 本帧刚按下
        bool mouseJustReleased{ false }; // 本帧刚抬起
        juce::Point<float> mouseDelta{ 0.0f, 0.0f }; // 自上一帧的位移

        // ---- 视口（像素） ----
        int viewportW{ 1 };
        int viewportH{ 1 };
    };
}
