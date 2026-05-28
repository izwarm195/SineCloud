#pragma once

#include <JuceHeader.h>

class SineCloudVoice
{
public:
    SineCloudVoice() = default;

    void prepareToPlay(double sampleRate);

    // 设置当前的"音池上下文"（每次外部改变这些参数就调一次）
    struct VoiceParams
    {
        float lowNote;     // Pitch 旋钮值（Csound: kLow）
        float minGap;      // 最小等待间隔（秒）
        float maxGap;      // 最大等待间隔（秒）
        float attackSec;
        float sustainSec;
        float releaseSec;
        float decaySec;
        int   currentRoot; // 当前根音（MIDI mod 12）
    };

    // 渲染时传入参数（参数会动态变化，不每次拷贝）
    void renderToBuffer(juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples,
        const VoiceParams& params);

    void silence()
    {
        state = State::Waiting;
        waitElapsed = 0.0;
        currentAngle = 0.0;
        // 随机错开下一次触发，避免 12 voice 齐发
        waitDuration = 0.05 + random.nextDouble() * 1.5;
    }


private:

    enum class State { Waiting, Playing };

    double currentSampleRate{ 48000.0 };

    State state{ State::Waiting };
    double waitTimer{ 0.0 };
    double waitDuration{ 0.5 };
    double waitElapsed{ 0.0 };
    double phase{ 0.0 };
    double duration{ 0.0 };

    // 当前粒子参数
    double currentAngle{ 0.0 };
    double angleDelta{ 0.0 };
    double currentMidi{ 60.0 };
    double targetMidi{ 60.0 };
    double startMidi{ 60.0 };
    double curAttack{ 1.0 };
    double curSustain{ 0.5 };
    double curRelease{ 1.0 };
    double curDecay{ 0.001 };
    bool   hasDecay{ false };
    double amplitude{ 0.1 };

    juce::Random random;

    // 内部辅助
    void triggerNewParticle(const VoiceParams& params);
    double calculateEnvelope() const;
};
