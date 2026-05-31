/*
  ==============================================================================
    KnobEntity.h
    Layer 3: Game / Interaction
    µØÃæÐýÅ¥£ºµ××ùÔ²Öù + ¶¥¸Ç + Ö¸ÕëÌõ¡£
    ÊýÖµ±ä»¯Ö»¸Ä pointerYaw£¨model ¾ØÕóÐý×ª£©£¬shader ×ÔÈ»Í¸ÊÓ¡£
    ½»»¥£ºÍæ¼Ò¿¿½ü -> Focused£»Êó±ê°´ÏÂÃüÖÐ¶¥¸Ç -> Dragging£¨Î¹ InertialValue£©¡£
    ÊýÖµÍ¬²½£ºInertialValue.onValueChanged -> ParamBridge.write
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
            // ÓÃ ParamBridge ÍÆ¶Ï·¶Î§
            const auto rng = bridge.getRange();
            inertia.setRange(rng.start, rng.end);
            inertia.setDragRangePixels(400.0f);
            inertia.setFrictionPerSecond(0.05f);
            inertia.setInertiaGain(0.6f);

            // µ±Ç°ÕæÊµÖµ -> InertialValue
            inertia.setValueFromHost(bridge.read());

            // InertialValue -> APVTS
            inertia.onValueChanged = [this](float v) { bridge.write(v); };

            // APVTS£¨host ×Ô¶¯»¯ / preset£©-> InertialValue
            bridge.onHostChanged = [this](float v) { inertia.setValueFromHost(v); };
        }

        //----------------------------------------------------------------------
        // ¹²Ïí mesh£¨World Ìá¹©£©
        //----------------------------------------------------------------------
        void setMeshes(Mesh* baseCylinder, Mesh* pointerBox) noexcept
        {
            cylMesh = baseCylinder;
            ptrMesh = pointerBox;
        }

        //----------------------------------------------------------------------
        // ¼¸ºÎ
        //----------------------------------------------------------------------
        void setRadius(float r) noexcept { radius = juce::jmax(0.05f, r); }
        void setHeight(float h) noexcept { height = juce::jmax(0.05f, h); }

        float getRadius() const noexcept { return radius; }
        float getHeight() const noexcept { return height; }

        //----------------------------------------------------------------------
        // ×´Ì¬
        //----------------------------------------------------------------------
        bool isFocused()  const noexcept { return focused; }
        bool isDragging() const noexcept { return dragging; }

        const juce::String& getLabel() const noexcept { return label; }
        float getValueNormalised() const noexcept { return inertia.getNormalised(); }

        //----------------------------------------------------------------------
        // Ê°È¡£ºÍæ¼ÒÎ»ÖÃ / Êó±êÉäÏß
        //----------------------------------------------------------------------
        bool isWithinReach(const Vec3& playerPos, float reach) const noexcept
        {
            const float dx = playerPos.x - worldPos.x;
            const float dy = playerPos.y - worldPos.y;
            return dx * dx + dy * dy <= reach * reach;
        }

        /** ¼òµ¥°üÎ§Çò / Ô²ÖùÊ°È¡£¨¶¥¸ÇÇøÓò£©¡£ */
        bool intersectsRay(const Ray& ray) const noexcept
        {
            // °ÑÐýÅ¥¿´×÷ z ¡Ê [0, height + capHeight] µÄÊúÖ±Ô²Öù£¨°ë¾¶ radius£©
            // Çó ray ÓëÎÞÏÞÔ²Öù£¨ÖáÑØ z£©µÄ½»µã£¬ÔÙ clamp z
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
        // ÓÉ World ÍÆ¶¯
        //----------------------------------------------------------------------
        /** Íæ¼ÒÊÇ·ñÔÚ½»»¥¾àÀëÄÚ£¨¾ö¶¨ Focused ×´Ì¬£©¡£ */
        void setFocused(bool f) noexcept { focused = f; }

      
        /*void beginMouseDrag() noexcept
        {
            dragging = true;
            inertia.beginDrag();
        }
        void onMouseDragDelta(juce::Point<float> delta) noexcept
        {
            if (!dragging) return;
            inertia.onDragDelta(delta.x, delta.y);
        }*/

        void endMouseDrag() noexcept
        {
            if (!dragging) return;
            dragging = false;
            inertia.endDrag();
        }
        /** 滚轮微调参数值，一个滚轮刻度 ≈ 5% 的量程 */
        void onMouseWheel(float deltaY) noexcept
        {
            // 利用现有 InertialValue 的拖拽通路：20px/刻度，400px = 满量程 → 每刻度 5%
            inertia.beginDrag();
            inertia.onDragDelta(0.0f, deltaY * 20.0f);
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

            // ---- µ××ùÔ²Öù£ºz ¡Ê [0, height]£¬°ë¾¶ radius ----
            // ¹²Ïí mesh ÊÇ createCylinder(0.5f, 1.0f, segs)£¬ËùÒÔ¶ÔÎ»ÓÃ scale(radius*2, ...)
            // µ« createCylinder Ö±½ÓÊÇ (radius, height) ÕæÊµ³ß´ç£¬World ÄÇ±ß´´½¨Ê±°´ 1.0/1.0 -> ÕâÀï·Ç¾ùÔÈËõ·Å
            const Mat4 baseModel = translation(worldPos)
                * rotationZ(yaw)
                * scaling({ radius, radius, height });
            const Vec3 baseColor = focused ? Vec3{ 0.45f, 0.55f, 0.70f }
            : Vec3{ 0.30f, 0.35f, 0.42f };
            const Vec3 emissive = focused ? Vec3{ 0.10f, 0.14f, 0.20f }
            : Vec3{ 0, 0, 0 };
            r.drawMesh(*cylMesh, baseModel, baseColor, emissive);

            // ---- ¶¥¸Ç£ºÔÚ height Ö®ÉÏµþÒ»¸ö°«Ô²Öù ----
            const Mat4 capModel = translation({ worldPos.x, worldPos.y, worldPos.z + height })
                * rotationZ(yaw)
                * scaling({ radius * 0.95f, radius * 0.95f, capHeight });
            r.drawMesh(*cylMesh, capModel,
                focused ? Vec3{ 0.80f, 0.80f, 0.85f }
            : Vec3{ 0.65f, 0.65f, 0.70f });

            // ---- Ö¸Õë£º´ÓÖÐÐÄÖ¸Ïò"Ç°"£¬Ðý×ª½Ç¸ù¾Ý¹éÒ»»¯Öµ ----
            // ÊýÖµ 0~1 Ó³Éäµ½ [-135¡ã, +135¡ã]
            const float t = inertia.getNormalised();
            const float ptrAngle = juce::degreesToRadians(-135.0f)
                + juce::degreesToRadians(270.0f) * t;

            // Ö¸ÕëÌõ£º³¤ = 0.85*radius£¬¿í/ºñ = ºÜ±¡£»ÖÐÐÄÔÚ¶¥¸ÇÉÏ·½Ò»µã
            const float pLen = radius * 0.85f;
            const float pThk = radius * 0.10f;

            const Mat4 ptrModel = translation({ worldPos.x,
                                                worldPos.y,
                                                worldPos.z + height + capHeight + pThk * 0.5f })
                * rotationZ(yaw + ptrAngle)
                // Ö¸ÕëÌõÔÚ +X ·½ÏòÑÓÉì£ºÏÈÆ½ÒÆµ½×ÔÉíÖÐµã
                * translation({ pLen * 0.5f, 0.0f, 0.0f })
                * scaling({ pLen, pThk, pThk });
            r.drawMesh(*ptrMesh, ptrModel, { 0.95f, 0.95f, 0.98f },
                focused ? Vec3{ 0.20f, 0.20f, 0.20f } : Vec3{ 0, 0, 0 });
        }

    private:
        // ÇÅ½Ó
        ParamBridge   bridge;
        InertialValue inertia;
        juce::String  label;

        // ¼¸ºÎ²ÎÊý
        float radius{ 0.6f };
        float height{ 0.4f };
        float capHeight{ 0.15f };

        // ¹²Ïí mesh
        Mesh* cylMesh{ nullptr };
        Mesh* ptrMesh{ nullptr };

        // ×´Ì¬
        bool focused{ false };
        bool dragging{ false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobEntity)
    };
}
