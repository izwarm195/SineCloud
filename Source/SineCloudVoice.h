/*
  ==============================================================================

    SineCloudVoice.h
    Created: 28 May 2026 9:38:50am
    Author:  wzm

  ==============================================================================
*/

#pragma once


#include <JuceHeader.h>

class SineCloudVoice : public juce::SynthesiserVoice
{
public:
    SineCloudVoice();

    // 判断这个 voice 能否响这个 sound
    bool canPlaySound(juce::SynthesiserSound* sound) override;

    // 按键时调用
    void startNote(int midiNoteNumber,
                   float velocity,
                   juce::SynthesiserSound* sound,
                   int currentPitchWheelPosition) override;

    // 松键时调用
    void stopNote(float velocity, bool allowTailOff) override;

    // 弯音轮（先空实现）
    void pitchWheelMoved(int newPitchWheelValue) override {}

    // 控制器（先空实现）
    void controllerMoved(int controllerNumber, int newControllerValue) override {}

    // 真正生成声音的地方
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample,
                         int numSamples) override;

    // 准备工作（拿到采样率）
    void prepareToPlay(double sampleRate, int samplesPerBlock, int outputChannels);

private:
    double currentSampleRate { 48000.0 };

    // 振荡器状态
    double currentAngle      { 0.0 };
    double angleDelta        { 0.0 };
    double level             { 0.0 };

    // ADSR
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
};
