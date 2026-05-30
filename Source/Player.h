/*
  ==============================================================================
    Player.h
    Layer 3: Game / Interaction
    Íæ¼Ò£º·½Ïò¼ü / WASD ÑØÏà»ú forward ÒÆ¶¯£¬×Ô´ø¼ÓËÙ + ×èÄá£¨ÑØÓÃ¾É IsoSceneDemo ÐÐÎª£©¡£
    »æÖÆ£ºÒ»¸öºìÉ«Á¢·½Ìå£¨¹²Ïí World Ìá¹©µÄ boxMesh£©¡£
  ==============================================================================*/
#pragma once

#include "Entity.h"
#include "InputState.h"
#include "Easing.h"

namespace sc
{
    class Player : public Entity //继承Entity
    {
    public:
        Player()
        {
            worldPos = { 0.0f, 0.0f, 0.0f };
            scale = { 0.6f, 0.6f, 0.6f };
        }

        void setMesh(Mesh* sharedBox) noexcept { boxMesh = sharedBox; }

        /** µ±Ç°Ö¡ËÙ¶È£¨Ìá¹©¸øÏà»ú×öÈí¸úËæÅÐ¶¨ÓÃ£©¡£ */
        Vec3 getVelocity() const noexcept { return velocity; }

        /** ÓÉ World ×¢Èë£ºµ±Ç°Ïà»úµÄË®Æ½Ãæ forward / right£¬ÓÃÓÚ"ÑØÏà»ú·½ÏòÒÆ¶¯"¡£ */
        void setBasis(const Vec3& forward, const Vec3& right) noexcept
        {
            camForward = forward;
            camRight = right;
        }

        // Player.h - update() 
        void update(float dt, const InputState& in) override
        {
            // ---- WASD控制方向 ----
            Vec3 input{ 0, 0, 0 };
            if (in.keyUp)    input = input + camForward;
            if (in.keyDown)  input = input - camForward;
            if (in.keyRight) input = input + camRight;
            if (in.keyLeft)  input = input - camRight;

            input.z = 0.0f;

            const float len2 = input.x * input.x + input.y * input.y;
            const bool hasInput = len2 > 0.01f;  // [¸Ä] ãÐÖµ¸üºÏÀí
            if (hasInput) input = normalize({ input.x, input.y, 0.0f });

            // ---- 目标速度向量 ----
            const Vec3 targetVel = 
                hasInput ? Vec3{ input.x * targetSpeed, input.y * targetSpeed, 0.0f }: Vec3{ 0, 0, 0 };

            // 加速
            const float accelRate = hasInput ? 0.999f : 0.999f;  // ¸ü´óµÄÖµ = ¸ü¿ì·´Ó¦
            velocity.x = easing::damp(velocity.x, targetVel.x, accelRate, dt);
            velocity.y = easing::damp(velocity.y, targetVel.y, accelRate, dt);

            // 归零
            if (!hasInput)
            {
                if (std::abs(velocity.x) < 0.05f) velocity.x = 0.0f;
                if (std::abs(velocity.y) < 0.05f) velocity.y = 0.0f;
            }

            // ---- 移动 ----
            worldPos.x += velocity.x * dt;
            worldPos.y += velocity.y * dt;

            // ---- 朝向 ----
            if (hasInput)
            {
                const float targetYaw = std::atan2(input.y, input.x);
                yaw = easing::damp(yaw, targetYaw, 2.0f, dt);
            }
        }

        void draw(Renderer& r, const Camera&) override
        {
            if (!visible || boxMesh == nullptr) return;

            // Ì§¸ß°ë¸öÉíÎ»£¬ÈÃÁ¢·½ÌåµÄµ×ÃæÌùµØ£¨box ÖÐÐÄÔÚ z=0£©
            const Mat4 model = translation({ worldPos.x, worldPos.y, scale.z * 0.5f })
                * rotationZ(yaw)
                * scaling(scale);
            r.drawMesh(*boxMesh, model, { 0.85f, 0.30f, 0.30f });
        }

        //// ---- 速度 ----
        float targetSpeed{ 5.0f };   // ¾ÉÖµÊÇÃ¿Ö¡ 5.0 ÏñËØ ¡Ö 300px/s£¬ÕâÀï»»³ÉÊÀ½çµ¥Î»
        

    private:
        Vec3 velocity{ 0, 0, 0 };
        Vec3 camForward{ 0, 1, 0 };
        Vec3 camRight{ 1, 0, 0 };

        Mesh* boxMesh{ nullptr }; // ÓÉ World ¹²Ïí
    };
}
