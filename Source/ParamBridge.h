/*
  ==============================================================================
    ParamBridge.h
    Bridge: APVTS <-> Knob/InertialValue
    ·â×°Ò»¸ö APVTS ²ÎÊý£º
      - read()  ·µ»ØÕæÊµÖµ£¨µ¥Î»Óë layout Ò»ÖÂ£¬ÀýÈç ms¡¢°ëÒô¡¢0~1£©
      - write() Ð´ÈëÕæÊµÖµ²¢Í¨ÖªËÞÖ÷
      - ¼àÌýËÞÖ÷¶Ë±ä»¯£¨×Ô¶¯»¯¡¢preset »Ö¸´£©£¬Í¨¹ý onHostChanged Å×»ØÈ¥
    UI Ïß³Ì¹¹Ôì£¬UI Ïß³ÌÎö¹¹£»ËùÓÐµ÷ÓÃ¶¼ÔÚ UI Ïß³Ì¡£
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
        // ÕæÊµÖµ¶ÁÐ´
        //----------------------------------------------------------------------
        float read() const noexcept
        {
            if (auto* atomic = apvts.getRawParameterValue(id))
                return atomic->load();
            return 0.0f;
        }

        /** Ð´ÕæÊµÖµ£¨jitter ÒÑ clamp µ½ layout µÄ range£©£¬Í¨ÖªËÞÖ÷¡£ */
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
        // ·¶Î§
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
        // host ¶Ë±ä»¯Í¨Öª£¨×Ô¶¯»¯ / preset »Ö¸´£©
        //----------------------------------------------------------------------
        std::function<void(float /*newRealValue*/)> onHostChanged;

    private:
        void parameterChanged(const juce::String& /*paramID*/, float newValue) override
        {
            // newValue ÊÇÕæÊµÖµ£¨²»ÊÇ¹éÒ»»¯£©£¬Óë read() Ò»ÖÂ
            if (onHostChanged)
            {
                // Õâ¸ö»Øµ÷À´×ÔËÞÖ÷Ïß³Ì£¬×ª»Ø message thread ÔÙ´¥·¢ UI
                juce::MessageManager::callAsync(
                    [cb = onHostChanged, v = newValue]() { cb(v); });
            }
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String id;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamBridge)
    };
}
