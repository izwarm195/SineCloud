/*
  ==============================================================================
    KnobEntity.h
    Layer 3: Game / Interaction
    地面旋钮：底座圆柱 + 顶盖 + 指针条。
    数值变化只改 pointerYaw（model 矩阵旋转），shader 自然透视。
    交互：玩家靠近 -> Focused；鼠标按下命中顶盖 -> Dragging（喂 InertialValue）。
    数值同步：InertialValue.onValueChanged -> ParamBridge.write
              ParamBridge.onHostChanged   -> InertialValue.setValueFromHost
  ==============================================================================*/
#pragma once

#include "Entity.h"
#include "InputState.h"
#include "InertialValue.h"
#include "ParamBridge.h"

namespace sc
{
    class KnobEntity : public Entity
    {
    public:
        KnobEntity(juce::AudioProcessorValueTreeState& apvts,
            const juce::String& paramId,
            const juce::String& displayName)
            : bridge(apvts, paramId), label(displayName)
        {
            // 用 ParamBridge 推断范围
            const auto rng = bridge.getRange();
            inertia.setRange(rng.start, rng.end);
            inertia.setDragRangePixels(400.0f);
            inertia.setFrictionPerSecond(0.05f);
            inertia.setInertiaGain(0.6f);

            // 当前真实值 -> InertialValue
            inertia.setValueFromHost(bridge.read());

            // InertialValue -> APVTS
            inertia.onValueChanged = [this](float v) { bridge.write(v); };

            // APVTS（host 自动化 / preset）-> InertialValue
            bridge.onHostChanged = [this](float v) { inertia.setValueFromHost(v); };
        }

        //----------------------------------------------------------------------
        // 共享 mesh（World 提供）
        //----------------------------------------------------------------------
        void setMeshes(Mesh* baseCylinder, Mesh* pointerBox) noexcept
        {
            cylMesh = baseCylinder;
            ptrMesh = pointerBox;
        }

        //----------------------------------------------------------------------
        // 几何
        //----------------------------------------------------------------------
        void setRadius(float r) noexcept { radius = juce::jmax(0.05f, r); }
        void setHeight(float h) noexcept { height = juce::jmax(0.05f, h); }

        float getRadius() const noexcept { return radius; }
        float getHeight() const noexcept { return height; }

        //----------------------------------------------------------------------
        // 状态
        //----------------------------------------------------------------------
        bool isFocused()  const noexcept { return focused; }
        bool isDragging() const noexcept { return dragging; }

        const juce::String& getLabel() const noexcept { return label; }
        float getValueNormalised() const noexcept { return inertia.getNormalised(); }

        //----------------------------------------------------------------------
        // 拾取：玩家位置 / 鼠标射线
        //----------------------------------------------------------------------
        bool isWithinReach(const Vec3& playerPos, float reach) const noexcept
        {
            const float dx = playerPos.x - worldPos.x;
            const float dy = playerPos.y - worldPos.y;
            return dx * dx + dy * dy <= reach * reach;
        }

        /** 简单包围球 / 圆柱拾取（顶盖区域）。 */
        bool intersectsRay(const Ray& ray) const noexcept
        {
            // 把旋钮看作 z ∈ [0, height + capHeight] 的竖直圆柱（半径 radius）
            // 求 ray 与无限圆柱（轴沿 z）的交点，再 clamp z
            const float ox = ray.origin.x - worldPos.x;
            const float oy = ray.origin.y - worldPos.y;
            const float dx = ray.direction.x;
            const float dy = ray.direction.y;

            const float a = dx * dx + dy * dy;
            if (a < 1.0e-6f) return false;
            const float b = 2.0f * (ox * dx + oy * dy);
            const float c = ox * ox + oy * oy - radius * radius;
            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f) return false;

            const float sq = std::sqrt(disc);
            const float t0 = (-b - sq) / (2.0f * a);
            const float t1 = (-b + sq) / (2.0f * a);
            const float t = (t0 > 0.0f) ? t0 : t1;
            if (t <= 0.0f) return false;

            const float zHit = ray.origin.z + ray.direction.z * t;
            const float topZ = worldPos.z + height + capHeight;
            return zHit >= worldPos.z && zHit <= topZ;
        }

        //----------------------------------------------------------------------
        // 由 World 推动
        //----------------------------------------------------------------------
        /** 玩家是否在交互距离内（决定 Focused 状态）。 */
        void setFocused(bool f) noexcept { focused = f; }

        /** 鼠标按下命中本旋钮 -> 进入拖拽。 */
        void beginMouseDrag() noexcept
        {
            dragging = true;
            inertia.beginDrag();
        }

        /** 拖拽过程中的 mouse delta（像素）。 */
        void onMouseDragDelta(juce::Point<float> delta) noexcept
        {
            if (!dragging) return;
            inertia.onDragDelta(delta.x, delta.y);
        }

        void endMouseDrag() noexcept
        {
            if (!dragging) return;
            dragging = false;
            inertia.endDrag();
        }

        //----------------------------------------------------------------------
        // Entity
        //----------------------------------------------------------------------
        void update(float dt, const InputState&) override
        {
            inertia.tick(dt);
        }

        void draw(Renderer& r, const Camera&) override
        {
            if (!visible || cylMesh == nullptr || ptrMesh == nullptr) return;

            // ---- 底座圆柱：z ∈ [0, height]，半径 radius ----
            // 共享 mesh 是 createCylinder(0.5f, 1.0f, segs)，所以对位用 scale(radius*2, ...)
            // 但 createCylinder 直接是 (radius, height) 真实尺寸，World 那边创建时按 1.0/1.0 -> 这里非均匀缩放
            const Mat4 baseModel = translation(worldPos)
                * rotationZ(yaw)
                * scaling({ radius, radius, height });
            const Vec3 baseColor = focused ? Vec3{ 0.45f, 0.55f, 0.70f }
            : Vec3{ 0.30f, 0.35f, 0.42f };
            const Vec3 emissive = focused ? Vec3{ 0.10f, 0.14f, 0.20f }
            : Vec3{ 0, 0, 0 };
            r.drawMesh(*cylMesh, baseModel, baseColor, emissive);

            // ---- 顶盖：在 height 之上叠一个矮圆柱 ----
            const Mat4 capModel = translation({ worldPos.x, worldPos.y, worldPos.z + height })
                * rotationZ(yaw)
                * scaling({ radius * 0.95f, radius * 0.95f, capHeight });
            r.drawMesh(*cylMesh, capModel,
                focused ? Vec3{ 0.80f, 0.80f, 0.85f }
            : Vec3{ 0.65f, 0.65f, 0.70f });

            // ---- 指针：从中心指向"前"，旋转角根据归一化值 ----
            // 数值 0~1 映射到 [-135°, +135°]
            const float t = inertia.getNormalised();
            const float ptrAngle = juce::degreesToRadians(-135.0f)
                + juce::degreesToRadians(270.0f) * t;

            // 指针条：长 = 0.85*radius，宽/厚 = 很薄；中心在顶盖上方一点
            const float pLen = radius * 0.85f;
            const float pThk = radius * 0.10f;

            const Mat4 ptrModel = translation({ worldPos.x,
                                                worldPos.y,
                                                worldPos.z + height + capHeight + pThk * 0.5f })
                * rotationZ(yaw + ptrAngle)
                // 指针条在 +X 方向延伸：先平移到自身中点
                * translation({ pLen * 0.5f, 0.0f, 0.0f })
                * scaling({ pLen, pThk, pThk });
            r.drawMesh(*ptrMesh, ptrModel, { 0.95f, 0.95f, 0.98f },
                focused ? Vec3{ 0.20f, 0.20f, 0.20f } : Vec3{ 0, 0, 0 });
        }

    private:
        // 桥接
        ParamBridge   bridge;
        InertialValue inertia;
        juce::String  label;

        // 几何参数
        float radius{ 0.6f };
        float height{ 0.4f };
        float capHeight{ 0.15f };

        // 共享 mesh
        Mesh* cylMesh{ nullptr };
        Mesh* ptrMesh{ nullptr };

        // 状态
        bool focused{ false };
        bool dragging{ false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobEntity)
    };
}
