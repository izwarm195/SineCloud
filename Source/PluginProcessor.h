/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SineCloudVoice.h"
#include "SineCloudSound.h"


//==============================================================================
/**
*/
class SineCloudAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SineCloudAudioProcessor();
    ~SineCloudAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    static constexpr int numVoices = 12;

    // maj9(#13) 音池半音偏移
    static constexpr int kIntervals[numVoices] = {
        0, 4, 7, 11, 14, 18, 21, 24, 28, 31, 35, 38
    };

    std::array<SineCloudVoice, numVoices> voices;

    // 全局 ADSR：控制整片云的进出
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    double currentSampleRate{ 48000.0 };

    // 当前根音
    int currentRootMidi{ -1 };

    // 粒子触发器
    double samplesUntilNextTrigger{ 0.0 };
    double samplesPerTrigger{ 48000.0 };  // = sampleRate / density

    // 参数（先用普通成员变量，后面接 APVTS）
    float pitchOffset{ 0.0f };   // -24 ~ +24 半音
    float density{ 4.0f };   // 0.2 ~ 32 Hz（每秒触发次数）

    // 随机数生成器
    juce::Random random;

    // MIDI 处理
    void handleMidiMessage(const juce::MidiMessage& msg);
    void triggerParticles(int blockOffset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessor)
};
