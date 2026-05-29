/*
  ==============================================================================
    Entity.h
    Layer 3: Game / Interaction
    场景内可被 World 管理的对象抽象：每帧 update(dt, input)，每帧 draw。
    具体世界变换由派生类决定如何使用 worldPos / yaw（不强制使用四元数）。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "Vec.h"
#include "Mat4.h"
#include "Renderer.h"
#include "Camera.h"

namespace sc
{
    struct InputState; // 前置声明

    class Entity
    {
    public:
        virtual ~Entity() = default;

        virtual void update(float /*dtSec*/, const InputState& /*in*/) {}
        virtual void draw(Renderer& /*r*/, const Camera& /*cam*/) {}

        Vec3  worldPos{ 0.0f, 0.0f, 0.0f };
        float yaw{ 0.0f };  // 绕 +Z
        Vec3  scale{ 1.0f, 1.0f, 1.0f };

        bool  visible{ true };

    protected:
        Mat4 modelMatrix() const noexcept
        {
            return translation(worldPos)
                * rotationZ(yaw)
                * scaling(scale);
        }
    };
}
