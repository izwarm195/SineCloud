/*
  ==============================================================================

    SineCloudVoice.cpp
    Created: 28 May 2026 9:38:42am
    Author:  wzm

  ==============================================================================
*/

#include "SineCloudVoice.h"
#include "SineCloudSound.h"

SineCloudVoice::SineCloudVoice()
{
    // 默认 ADSR：快速起音，长延音，慢释放（适合 Sine Cloud 那种 ambient 感）
    adsrParams.attack  = 0.05f;   // 50 ms
    adsrParams.decay   = 0.1f;    // 100 ms
    adsrParams.sustain = 0.8f;    // 80% 电平
    adsrParams.release = 1.5f;    // 1.5 秒
    adsr.setParameters(adsrParams);
}

bool SineCloudVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SineCloudSound*>(sound) != nullptr;
}

void SineCloudVoice::prepareToPlay(double sampleRate, int /*samplesPerBlock*/, int /*outputChannels*/)
{
    currentSampleRate = sampleRate;
    adsr.setSampleRate(sampleRate);
}

void SineCloudVoice::startNote(int midiNoteNumber, float velocity,
    juce::SynthesiserSound* /*sound*/,
    int /*currentPitchWheelPosition*/)
{
    double freqHz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    double cyclesPerSample = freqHz / currentSampleRate;
    angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;

    level = velocity * 0.15;

    currentAngle = 0.0;   // ← 加这行
    adsr.reset();
    adsr.noteOn();
}


void SineCloudVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();   // 进入 release 阶段，自然衰减
    }
    else
    {
        clearCurrentNote();
        adsr.reset();
        angleDelta = 0.0;
    }
}

void SineCloudVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
    int startSample,
    int numSamples)
{
    if (angleDelta == 0.0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // 生成正弦波样本
        float currentSample = (float)(std::sin(currentAngle) * level);

        // 套上 ADSR 包络
        float envValue = adsr.getNextSample();
        currentSample *= envValue;

        // 写入所有输出通道
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample(channel, startSample + sample, currentSample);

        // 推进相位
        currentAngle += angleDelta;
        if (currentAngle > juce::MathConstants<double>::twoPi)
            currentAngle -= juce::MathConstants<double>::twoPi;

        // ADSR 结束清除 voice
        if (!adsr.isActive())
        {
            clearCurrentNote();
            angleDelta = 0.0;
            break;
        }
    }
}

