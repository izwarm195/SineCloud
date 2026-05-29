/*
  ==============================================================================
    ParamBridge.h
    Bridge: APVTS <-> Knob/InertialValue
    封装一个 APVTS 参数：
      - read()  返回真实值（单位与 layout 一致，例如 ms、半音、0~1）
      - write() 写入真实值并通知宿主
      - 监听宿主端变化（自动化、preset 恢复），通过 onHostChanged 抛回去
    UI 线程构造，UI 线程析构；所有调用都在 UI 线程。
  ==============================================================================*/
#pragma once

#include <JuceHeader.h>
#include <functional>

namespace sc
{
    class ParamBridge : private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        ParamBridge(juce::AudioProcessorValueTreeState& apvtsRef,
            const juce::String& paramId)
            : apvts(apvtsRef), id(paramId)
        {
            apvts.addParameterListener(id, this);
        }

        ~ParamBridge() override
        {
            apvts.removeParameterListener(id, this);
        }

        //----------------------------------------------------------------------
        // 真实值读写
        //----------------------------------------------------------------------
        float read() const noexcept
        {
            if (auto* atomic = apvts.getRawParameterValue(id))
                return atomic->load();
            return 0.0f;
        }

        /** 写真实值（jitter 已 clamp 到 layout 的 range），通知宿主。 */
        void write(float realValue) noexcept
        {
            if (auto* p = apvts.getParameter(id))
            {
                const auto range = getRange();
                const float clamped = juce::jlimit(range.start, range.end, realValue);
                const float norm = range.convertTo0to1(clamped);
                p->setValueNotifyingHost(norm);
            }
        }

        //----------------------------------------------------------------------
        // 范围
        //----------------------------------------------------------------------
        juce::NormalisableRange<float> getRange() const
        {
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
                return p->getNormalisableRange();
            return { 0.0f, 1.0f };
        }

        float getMin() const { return getRange().start; }
        float getMax() const { return getRange().end; }

        //----------------------------------------------------------------------
        // host 端变化通知（自动化 / preset 恢复）
        //----------------------------------------------------------------------
        std::function<void(float /*newRealValue*/)> onHostChanged;

    private:
        void parameterChanged(const juce::String& /*paramID*/, float newValue) override
        {
            // newValue 是真实值（不是归一化），与 read() 一致
            if (onHostChanged)
            {
                // 这个回调来自宿主线程，转回 message thread 再触发 UI
                juce::MessageManager::callAsync(
                    [cb = onHostChanged, v = newValue]() { cb(v); });
            }
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String id;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamBridge)
    };
}
