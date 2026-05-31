/* ==============================================================================
   KnobEntity.h   Layer 3: Game / Interaction

   Cylinder + pointer knob in the 3D world.

   Interaction:
     - Looking at a knob → focused (highlighted)
     - Mouse wheel over a focused knob → nudge parameter target
     - InertialValue smoothly chases the target (visible on the pointer)

   No mouse-drag interaction.  Wheel-only, with smooth glide.
   ============================================================================== */

#pragma once

#include "Entity.h"
#include "InputState.h"
#include "InertialValue.h"
#include "ParamBridge.h"

namespace sc {

    class KnobEntity : public Entity
    {
    public:
        KnobEntity(juce::AudioProcessorValueTreeState& apvts,
            const juce::String& paramId,
            const juce::String& displayName)
            : bridge(apvts, paramId)
            , label(displayName)
        {
            const auto rng = bridge.getRange();
            inertia.setRange(rng.start, rng.end);
            inertia.setSmoothSpeed(10.0f);          // gentle glide — tweak to taste

            // Current host value → InertialValue
            inertia.setValueFromHost(bridge.read());

            // InertialValue → APVTS
            inertia.onValueChanged = [this](float v)
                {
                    bridge.write(v);
                };

            // Host automation / preset → InertialValue
            bridge.onHostChanged = [this](float v)
                {
                    inertia.setValueFromHost(v);
                };
        }

        // ---- Meshes (supplied by World) ----

        void setMeshes(Mesh* baseCylinder, Mesh* pointerBox) noexcept
        {
            cylMesh = baseCylinder;
            ptrMesh = pointerBox;
        }

        // ---- Dimensions ----

        void  setRadius(float r) noexcept { radius = juce::jmax(0.05f, r); }
        void  setHeight(float h) noexcept { height = juce::jmax(0.05f, h); }
        float getRadius() const noexcept { return radius; }
        float getHeight() const noexcept { return height; }

        // ---- State ----

        bool isFocused() const noexcept { return focused; }
        bool isMoving()  const noexcept { return inertia.isMoving(); }
        const juce::String& getLabel() const noexcept { return label; }
        float getValueNormalised() const noexcept { return inertia.getNormalised(); }

        // ---- Picking ----

        bool isWithinReach(const Vec3& playerPos, float reach) const noexcept
        {
            const float dx = playerPos.x - worldPos.x;
            const float dy = playerPos.y - worldPos.y;
            return dx * dx + dy * dy <= reach * reach;
        }

        bool intersectsRay(const Ray& ray) const noexcept
        {
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

        // ---- Focus ----

        /** Called by World when the player is closest to this knob. */
        void setFocused(bool f) noexcept { focused = f; }

        // ---- Mouse wheel ----

        /** One wheel tick ≈ 5% of the parameter range.
            Value glides smoothly to the new target (see InertialValue::tick). */
        void onMouseWheel(float deltaY) noexcept
        {
            constexpr float stepPerTick = 0.1f;
            inertia.nudgeTarget(deltaY * stepPerTick);
        }

        // ---- Entity overrides ----

        void update(float dt, const InputState&) override
        {
            inertia.tick(dt);
        }

        void draw(Renderer& r, const Camera&) override
        {
            if (!visible || cylMesh == nullptr || ptrMesh == nullptr)
                return;

            // ---- Cylinder body ----
            const Mat4 baseModel = translation(worldPos)
                * rotationZ(yaw)
                * scaling({ radius, radius, height });

            const Vec3 baseColor = focused ? Vec3{ 0.45f, 0.55f, 0.70f }
            : Vec3{ 0.30f, 0.35f, 0.42f };
            const Vec3 emissive = focused ? Vec3{ 0.10f, 0.14f, 0.20f }
            : Vec3{ 0, 0, 0 };

            r.drawMesh(*cylMesh, baseModel, baseColor, emissive);

            // ---- Cap disc on top ----
            const Mat4 capModel = translation({ worldPos.x, worldPos.y, worldPos.z + height })
                * rotationZ(yaw)
                * scaling({ radius * 0.95f, radius * 0.95f, capHeight });

            r.drawMesh(*cylMesh, capModel,
                focused ? Vec3{ 0.80f, 0.80f, 0.85f }
            : Vec3{ 0.65f, 0.65f, 0.70f });

            // ---- Pointer: maps normalised [0..1] → angle [-135°, +135°] ----
            const float t = inertia.getNormalised();
            const float ptrAngle = juce::degreesToRadians(-135.0f)
                + juce::degreesToRadians(270.0f) * t;

            const float pLen = radius * 0.85f;
            const float pThk = radius * 0.10f;

            const Mat4 ptrModel = translation({
                                      worldPos.x,
                                      worldPos.y,
                                      worldPos.z + height + capHeight + pThk * 0.5f
                })
                * rotationZ(yaw + ptrAngle)
                * translation({ pLen * 0.5f, 0.0f, 0.0f })
                * scaling({ pLen, pThk, pThk });

            r.drawMesh(*ptrMesh, ptrModel,
                { 0.95f, 0.95f, 0.98f },
                focused ? Vec3{ 0.20f, 0.20f, 0.20f } : Vec3{ 0, 0, 0 });
        }

    private:
        ParamBridge   bridge;
        InertialValue inertia;
        juce::String  label;

        float radius{ 0.6f };
        float height{ 0.4f };
        float capHeight{ 0.15f };

        Mesh* cylMesh{ nullptr };
        Mesh* ptrMesh{ nullptr };

        bool focused{ false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobEntity)
    };

} // namespace sc
