/*
  ==============================================================================
    Player.h
    Layer 3: Game / Interaction
    玩家：方向键 / WASD 沿相机 forward 移动，自带加速 + 阻尼（沿用旧 IsoSceneDemo 行为）。
    绘制：一个红色立方体（共享 World 提供的 boxMesh）。
  ==============================================================================*/
#pragma once

#include "Entity.h"
#include "InputState.h"
#include "Easing.h"

namespace sc
{
    class Player : public Entity
    {
    public:
        Player()
        {
            worldPos = { 0.0f, 0.0f, 0.0f };
            scale = { 0.6f, 0.6f, 0.6f };
        }

        void setMesh(Mesh* sharedBox) noexcept { boxMesh = sharedBox; }

        /** 当前帧速度（提供给相机做软跟随判定用）。 */
        Vec3 getVelocity() const noexcept { return velocity; }

        /** 由 World 注入：当前相机的水平面 forward / right，用于"沿相机方向移动"。 */
        void setBasis(const Vec3& forward, const Vec3& right) noexcept
        {
            camForward = forward;
            camRight = right;
        }

        void update(float dt, const InputState& in) override
        {
            // ---- 输入方向 ----
            Vec3 input{ 0, 0, 0 };
            if (in.keyUp)    input = input + camForward;
            if (in.keyDown)  input = input - camForward;
            if (in.keyRight) input = input + camRight;
            if (in.keyLeft)  input = input - camRight;
            input.z = 0.0f;

            const float len2 = input.x * input.x + input.y * input.y;
            const bool hasInput = len2 > 1.0e-6f;
            if (hasInput) input = normalize({ input.x, input.y, 0.0f });

            // ---- 目标速度 + 加速/阻尼 ----
            const Vec3 targetVel = hasInput
                ? Vec3{ input.x * targetSpeed, input.y * targetSpeed, 0.0f }
            : Vec3{ 0, 0, 0 };

            const float accelRate = hasInput ? accelPerSec : dampPerSec;
            velocity.x = easing::damp(velocity.x, targetVel.x, accelRate, dt);
            velocity.y = easing::damp(velocity.y, targetVel.y, accelRate, dt);

            // 把非常小的残余直接归零，避免漂移
            if (!hasInput)
            {
                if (std::abs(velocity.x) < 0.02f) velocity.x = 0.0f;
                if (std::abs(velocity.y) < 0.02f) velocity.y = 0.0f;
            }

            // ---- 位置 ----
            worldPos.x += velocity.x * dt;
            worldPos.y += velocity.y * dt;

            // ---- 朝向：面向移动方向，否则保持 ----
            if (hasInput)
            {
                const float targetYaw = std::atan2(input.x, input.y); // 让 +Y 为 yaw=0
                yaw = easing::damp(yaw, targetYaw, 0.95f, dt);
            }
        }

        void draw(Renderer& r, const Camera&) override
        {
            if (!visible || boxMesh == nullptr) return;

            // 抬高半个身位，让立方体的底面贴地（box 中心在 z=0）
            const Mat4 model = translation({ worldPos.x, worldPos.y, scale.z * 0.5f })
                * rotationZ(yaw)
                * scaling(scale);
            r.drawMesh(*boxMesh, model, { 0.85f, 0.30f, 0.30f });
        }

        // ---- 调参（沿用旧 demo 的手感，单位：每秒） ----
        float targetSpeed{ 4.0f };   // 旧值是每帧 5.0 像素 ≈ 300px/s，这里换成世界单位
        float accelPerSec{ 0.99f };  // 越接近 1 加速越快（damp rate）
        float dampPerSec{ 0.98f };  // 松手减速

    private:
        Vec3 velocity{ 0, 0, 0 };
        Vec3 camForward{ 0, 1, 0 };
        Vec3 camRight{ 1, 0, 0 };

        Mesh* boxMesh{ nullptr }; // 由 World 共享
    };
}
