#pragma once

#include <JuceHeader.h>

class SineCloudVoice
{
public:
    SineCloudVoice() = default;

    void prepareToPlay(double sampleRate);

    // 触发一次粒子：设定音高 + 启动内部包络
    void triggerParticle(double midiNoteWithFraction, double initialLevel);

    // 渲染
    void renderToBuffer(juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples);

    bool isActive() const { return particleEnv > 0.0001; }

private:
    double currentSampleRate{ 48000.0 };
    double currentAngle{ 0.0 };
    double angleDelta{ 0.0 };
    double level{ 0.0 };

    // 粒子内部包络（指数衰减）
    double particleEnv{ 0.0 };
    double particleDecay{ 0.999 };  // 每个采样点的衰减系数
};
