/* ==============================================================================
   InertialValue.h   Layer 1: Math

   A floating-point value that smoothly chases a moving target.
   No drag state machine.  nudgeTarget() moves the target; tick() catches up.

   Feedback guard: writes that echo back from APVTS via onHostChanged are
   deduplicated so they don't snap the target.
   ============================================================================== */

#pragma once
#include <juce_core/juce_core.h>
#include <functional>

namespace sc {

    class InertialValue
    {
    public:
        InertialValue() = default;

        // ---- Configuration ----

        void setRange(float minValue, float maxValue) noexcept
        {
            jassert(maxValue > minValue);
            minV = minValue;
            maxV = maxValue;
            target = juce::jlimit(minV, maxV, target);
            value = juce::jlimit(minV, maxV, value);
        }

        /** How fast value catches up to target.
            5  = gentle (~0.8s settle)
            12 = responsive (~0.3s)
            25 = near-instant */
        void setSmoothSpeed(float speed) noexcept
        {
            smoothSpeed = juce::jmax(0.5f, speed);
        }

        // ---- Read ----

        float getValue()      const noexcept { return value; }
        float getTarget()     const noexcept { return target; }
        float getMin()        const noexcept { return minV; }
        float getMax()        const noexcept { return maxV; }

        float getNormalised() const noexcept
        {
            return (maxV > minV) ? (value - minV) / (maxV - minV) : 0.0f;
        }

        bool isMoving() const noexcept
        {
            return std::abs(value - target) > 0.0001f;
        }

        // ---- Write ----

        /** Host automation / preset change.  Instant snap both value & target.
            Feedback guard: if this matches our last outgoing write, skip —
            it's the APVTS echoing back, not a genuine host change. */
        void setValueFromHost(float newValue) noexcept
        {
            if (juce::approximatelyEqual(newValue, lastWrittenValue))
                return;                         // ★ dedup: don't snap target

            value = target = juce::jlimit(minV, maxV, newValue);
            lastWrittenValue = value;
        }

        /** Nudge the target by a normalised delta (0..1 scale).
            Value will glide there via tick(). */
        void nudgeTarget(float normalisedDelta) noexcept
        {
            const float range = maxV - minV;
            target = juce::jlimit(minV, maxV, target + normalisedDelta * range);
        }

        // ---- Per-frame ----

        void tick(float dtSec) noexcept
        {
            if (juce::approximatelyEqual(value, target))
                return;

            const float factor = 1.0f - std::exp(-smoothSpeed * juce::jmax(1.0e-4f, dtSec));
            value += (target - value) * factor;

            if (std::abs(value - target) < 0.0001f)
                value = target;

            if (onValueChanged)
            {
                lastWrittenValue = value;       // ★ record what we're about to echo
                onValueChanged(value);
            }
        }

        // ---- Callback ----

        std::function<void(float)> onValueChanged;

    private:
        float value{ 0.0f };
        float target{ 0.0f };
        float minV{ 0.0f };
        float maxV{ 1.0f };
        float smoothSpeed{ 1.0f };

        float lastWrittenValue{ -1.0e9f };     // sentinel: never matches any real value
    };

} // namespace sc
