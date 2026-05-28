#pragma once

#include <JuceHeader.h>

//==============================================================================
// 一个 Voice = 一颗自治粒子发生器
// 状态机：Waiting → Playing → Waiting
// Playing 内部包含 4 段包络：Attack → Sustain(+jitter) → Release(到0.3) → Decay(exp)
//==============================================================================
class SineCloudVoice
{
public:
    struct VoiceParams
    {
        float lowNote = 62.0f;   // Pitch 锚点 (MIDI)
        float minGap = 0.15f;   // 触发间隔下限 (秒)
        float maxGap = 0.8f;    // 触发间隔上限 (秒)
        float attackSec = 1.0f;
        float sustainSec = 0.5f;
        float releaseSec = 1.0f;
        float decaySec = 0.0f;
        float dream = 0.0f;    // 0~12
        float floatMs = 50.0f;   // Pitch/Pan 滑移时间 (ms)
        float shimmer = 0.0f;    // 起音上方半音数 (0~48)
        int   currentRoot = 0;       // 0~11
        const int* intervals = nullptr; // 指向 kIntervals[12]
        bool   gateOpen = true;   // false = 不再触发新粒子，但让正在响的走完

    };

    SineCloudVoice() = default;

    void prepareToPlay(double sampleRate);
    void setInitialWait(double seconds);

    // 渲染到立体声 buffer (累加，不清零)
    void renderToBuffer(juce::AudioBuffer<float>& buffer,
        int startSample,
        int numSamples,
        const VoiceParams& params);

private:
    enum class State { Waiting, Playing };

    void triggerNewParticle(const VoiceParams& p);

    double sampleRate{ 44100.0 };

    State  state{ State::Waiting };
    double waitDuration{ 0.5 };
    double waitElapsed{ 0.0 };
    double phase{ 0.0 };   // Playing 经过的秒数
    double duration{ 0.0 };   // 本颗粒子总时长

    // 当前粒子参数（触发时锁定一次）
    double targetMidi{ 60.0 };
    double startMidi{ 60.0 };  // = targetMidi + shimmer
    double curEnvTime{ 0.05 };  // = floatMs/1000
    double curAtk{ 1.0 };
    double curSus{ 0.5 };
    double curRel{ 1.0 };
    double curDcy{ 0.001 };
    bool   hasDcy{ false };
    double amplitude{ 0.08 };
    double susJitter{ 0.0 };
    double curDream{ 0.0 };

    double panStart{ 0.5 };
    double panEnd{ 0.5 };

    // 两个振荡器层各自的 up/down 失谐相位
    double angleBaseUp{ 0.0 };
    double angleBaseDn{ 0.0 };
    double angleL1Up{ 0.0 };
    double angleL1Dn{ 0.0 };

    juce::Random random;
};
