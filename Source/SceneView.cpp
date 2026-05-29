/*
  ==============================================================================
    SceneView.h
    Layer 2: Scene & Renderer
    GL 视口组件：拥有 OpenGLContext / Camera / Renderer，60Hz 主循环。
    World（Layer 3）尚未引入时，自带一个调试场景：地面网格 + 原点立方体 +
    一个旋转的小盒子，鼠标右键拖拽旋转视角，滚轮缩放。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Camera.h"
