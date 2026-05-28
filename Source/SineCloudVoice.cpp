#include "SineCloudVoice.h"

void SineCloudVoice::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate;

    // 粒子衰减时间常数：默认每秒衰减到 1/e（约 1 秒尾音）
    // 衰减系数 = exp(-1/(sampleRate * decayTimeSeconds))
    const double decayTimeSec = 1.0;
    particleDecay = std::exp(-1.0 / (sampleRate * decayTimeSec));
}

void SineCloudVoice::triggerParticle(double midiNoteWithFraction, double initialLevel)
{
    double freqHz = 440.0 * std::pow(2.0, (midiNoteWithFraction - 69.0) / 12.0);
    angleDelta = (freqHz / currentSampleRate) * juce::MathConstants<double>::twoPi;

    level = initialLevel;
    currentAngle = 0.0;       // 相位重置
    particleEnv = 1.0;        // 内部包络重启
}

void SineCloudVoice::renderToBuffer(juce::AudioBuffer<float>& outputBuffer,
    int startSample,
    int numSamples)
{
    if (angleDelta == 0.0 || !isActive())
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float currentSample = (float)(std::sin(currentAngle) * level * particleEnv);

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample(channel, startSample + sample, currentSample);

        currentAngle += angleDelta;
        if (currentAngle >= juce::MathConstants<double>::twoPi)
            currentAngle -= juce::MathConstants<double>::twoPi;

        // 指数衰减
        particleEnv *= particleDecay;
    }
}
