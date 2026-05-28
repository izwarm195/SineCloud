#include "SineCloudVoice.h"

void SineCloudVoice::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate;
}

void SineCloudVoice::setMidiNote(double midiNoteWithFraction)
{
    // 支持小数 MIDI 音高（用于以后 Dream 失谐）
    double freqHz = 440.0 * std::pow(2.0, (midiNoteWithFraction - 69.0) / 12.0);
    angleDelta = (freqHz / currentSampleRate) * juce::MathConstants<double>::twoPi;
}

void SineCloudVoice::renderToBuffer(juce::AudioBuffer<float>& outputBuffer,
    int startSample,
    int numSamples)
{
    if (angleDelta == 0.0 || level <= 0.0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float currentSample = (float)(std::sin(currentAngle) * level);

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample(channel, startSample + sample, currentSample);

        currentAngle += angleDelta;
        if (currentAngle >= juce::MathConstants<double>::twoPi)
            currentAngle -= juce::MathConstants<double>::twoPi;
    }
}
