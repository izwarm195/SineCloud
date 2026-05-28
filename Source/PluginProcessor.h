/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SineCloudVoice.h"

//==============================================================================
/**
*/
class SineCloudAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    SineCloudAudioProcessor();
    ~SineCloudAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // 公开 APVTS 让 Editor 能访问
    juce::AudioProcessorValueTreeState apvts;

    // 参数 ID 常量
    static constexpr const char* PARAM_PITCH = "pitch";
    static constexpr const char* PARAM_DENSITY = "density";
    static constexpr const char* PARAM_ATTACK = "attack";
    static constexpr const char* PARAM_SUSTAIN = "sustain";
    static constexpr const char* PARAM_RELEASE = "release";
    static constexpr const char* PARAM_DECAY = "decay";

private:
    //==============================================================================
    // 12 个自治 Voice Slot
    static constexpr int numVoices = 12;
    std::array<SineCloudVoice, numVoices> voices;

    // 当前根音 (0~11, C=0, C#=1, ...)
    int currentRoot{ 0 };

    // 内部方法
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleMidiMessage(const juce::MidiMessage& msg);

    bool noteHeld{ false };
    int  heldNote{ -1 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessor)
};
