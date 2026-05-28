#pragma once

#include <JuceHeader.h>

class SineCloudVoice
{
public:
    SineCloudVoice() = default;

    void prepareToPlay(double sampleRate);

    // 设置这个 voice 要发什么音（MIDI 音高，可带小数实现微调）
    void setMidiNote(double midiNoteWithFraction);

    // 设置音量（0~1）
    void setLevel(double newLevel) { level = newLevel; }

    // 渲染一个 block（不带 ADSR，纯正弦）
    void renderToBuffer(juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples);

    // 复位相位（重新触发时用）
    void resetPhase() { currentAngle = 0.0; }

    bool isActive() const { return level > 0.0; }

private:
    double currentSampleRate{ 48000.0 };
    double currentAngle{ 0.0 };
    double angleDelta{ 0.0 };
    double level{ 0.0 };
};
