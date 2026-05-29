/*
  ==============================================================================
    InertialValue.h
    Layer 1: Math
    脱离 juce::Slider / juce::Component 的惯性数值控制器：
      - 拖动：把鼠标 dx/dy/dt 喂进来，内部记录瞬时速度
      - 抬手：以瞬时速度滑行，按"每秒保留率"衰减直到停止
      - 自动化：宿主端改变参数时调用 setValueFromHost(...)，不会触发回调

    持有方（KnobEntity / ParamBridge）需要每帧调用 tick(dt)，并把 onValueChanged
    回调指向 ParamBridge::write()。
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include <functional>
#include <cmath>
#include <algorithm>

namespace sc
{
    class InertialValue
    {
    public:
        InertialValue() = default;

        //----------------------------------------------------------------------
        // 配置
        //----------------------------------------------------------------------
        /** 真实值范围（与 APVTS 参数 min/max 对齐）。 */
        void setRange(float minValue, float maxValue) noexcept
        {
            jassert(maxValue > minValue);
            minV = minValue;
            maxV = maxValue;
            value = juce::jlimit(minV, maxV, value);
        }

        /** 拖动灵敏度：拖满 dragRangePixels 像素 = 走完整个值域（min→max）。
            旧 InertialSlider 用 setMouseDragSensitivity(400)，这里默认就 400。 */
        void setDragRangePixels(float pixels) noexcept
        {
            dragRangePx = juce::jmax(1.0f, pixels);
        }

        /** 摩擦：每秒保留多少速度。0.05 = 1 秒后只剩 5% 速度（很涩），
            0.5 = 1 秒后剩 50% 速度（很滑）。旧值 friction=0.9（每帧）≈ 0.0024（每秒）
            过涩，这里默认设成 0.05，更接近"惯性手感"。 */
        void setFrictionPerSecond(float keepRatePerSecond) noexcept
        {
            frictionPerSec = juce::jlimit(1.0e-4f, 0.999f, keepRatePerSecond);
        }

        /** 抬手时把瞬时速度乘以这个倍数，作为初始滑行速度。 */
        void setInertiaGain(float g) noexcept
        {
            inertiaGain = juce::jmax(0.0f, g);
        }

        /** 速度低于此值（单位：值/秒）时停止滑行。 */
        void setStopThreshold(float t) noexcept
        {
            stopThreshold = juce::jmax(0.0f, t);
        }

        //----------------------------------------------------------------------
        // 数值访问
        //----------------------------------------------------------------------
        float getValue() const noexcept { return value; }
        float getMin()   const noexcept { return minV; }
        float getMax()   const noexcept { return maxV; }
        float getNormalised() const noexcept
        {
            return (maxV > minV) ? (value - minV) / (maxV - minV) : 0.0f;
        }
        bool  isDragging() const noexcept { return dragging; }
        bool  isCoasting() const noexcept { return std::abs(velocity) > stopThreshold; }

        /** 用户主动设置（会触发 onValueChanged）。 */
        void setValue(float newValue, bool notify = true) noexcept
        {
            const float clamped = juce::jlimit(minV, maxV, newValue);
            if (!juce::approximatelyEqual(clamped, value))
            {
                value = clamped;
                if (notify && onValueChanged)
                    onValueChanged(value);
            }
        }

        /** 宿主自动化 / preset 恢复回灌（不会回调，避免反向写入死循环）。 */
        void setValueFromHost(float newValue) noexcept
        {
            value = juce::jlimit(minV, maxV, newValue);
            // 来自宿主的变化应该清除惯性，否则手感和自动化会打架
            velocity = 0.0f;
        }

        //----------------------------------------------------------------------
        // 鼠标交互
        //----------------------------------------------------------------------
        void beginDrag() noexcept
        {
            dragging = true;
            velocity = 0.0f;
            instantVelocity = 0.0f;
            lastDragTimeMs = juce::Time::getMillisecondCounterHiRes();
        }

        /** 喂入本帧鼠标位移（像素）。约定：dy 向下为正，向上拖增大值（与旋钮直觉一致）。 */
        void onDragDelta(float dxPixels, float dyPixels) noexcept
        {
            if (!dragging) return;

            const double now = juce::Time::getMillisecondCounterHiRes();
            const float dtMs = (float)juce::jmax(1.0, now - lastDragTimeMs);
            lastDragTimeMs = now;

            // 像素 → 值：拖满 dragRangePx 像素 = 走完一个值域跨度
            const float range = maxV - minV;
            const float pxToVal = range / dragRangePx;

            // 向上拖（dy<0）+ 向右拖（dx>0）= 增大值
            const float deltaPx = (-dyPixels) + dxPixels;
            const float deltaVal = deltaPx * pxToVal;

            const float newValue = juce::jlimit(minV, maxV, value + deltaVal);
            const float actualDelta = newValue - value;
            value = newValue;

            // 记录瞬时速度（值/秒），用于 endDrag 时启动滑行
            instantVelocity = actualDelta / juce::jmax(1.0e-3f, dtMs * 0.001f);

            if (onValueChanged)
                onValueChanged(value);
        }

        void endDrag() noexcept
        {
            dragging = false;
            velocity = instantVelocity * inertiaGain;
            // 防止抖动：太小直接归零
            if (std::abs(velocity) < stopThreshold)
                velocity = 0.0f;
        }

        //----------------------------------------------------------------------
        // 每帧推进（由 SceneView / World 调用）
        //----------------------------------------------------------------------
        void tick(float dtSec) noexcept
        {
            if (dragging || velocity == 0.0f)
                return;

            // 帧率无关衰减：v *= frictionPerSec ^ dt
            velocity *= std::pow(frictionPerSec, juce::jmax(1.0e-4f, dtSec));

            const float newValue = juce::jlimit(minV, maxV, value + velocity * dtSec);

            // 撞到边界 → 立即停
            if (newValue <= minV || newValue >= maxV)
                velocity = 0.0f;

            if (!juce::approximatelyEqual(newValue, value))
            {
                value = newValue;
                if (onValueChanged)
                    onValueChanged(value);
            }

            if (std::abs(velocity) < stopThreshold)
                velocity = 0.0f;
        }

        //----------------------------------------------------------------------
        // 回调（KnobEntity 在构造时连到 ParamBridge::write）
        //----------------------------------------------------------------------
        std::function<void(float /*newRealValue*/)> onValueChanged;

    private:
        // 状态
        float value{ 0.0f };
        float velocity{ 0.0f };          // 值/秒
        float instantVelocity{ 0.0f };   // 拖动最后一帧的瞬时速度（值/秒）
        bool  dragging{ false };
        double lastDragTimeMs{ 0.0 };

        // 范围
        float minV{ 0.0f };
        float maxV{ 1.0f };

        // 调参
        float dragRangePx{ 400.0f };
        float frictionPerSec{ 0.05f };   // 1 秒衰减到 5%
        float inertiaGain{ 0.8f };
        float stopThreshold{ 1.0e-4f };  // 值/秒
    };
}
