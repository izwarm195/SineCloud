#pragma once

#include <JuceHeader.h>
#include "SineCloudVoice.h"
#include "InertialSlider.h"

//==============================================================================
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
    juce::AudioProcessorValueTreeState apvts;

    // ²ÎÊý ID ³£Á¿£¨¶ÔÆë Csound channel Ãû£©
    static constexpr const char* PARAM_PITCH = "lowNote";
    static constexpr const char* PARAM_DENSITY = "density";
    static constexpr const char* PARAM_ATTACK = "attack";
    static constexpr const char* PARAM_DECAY = "decay";
    static constexpr const char* PARAM_SUSTAIN = "sustain";
    static constexpr const char* PARAM_RELEASE = "release";
    static constexpr const char* PARAM_GAIN = "gain";
    static constexpr const char* PARAM_DREAM = "dream";
    static constexpr const char* PARAM_FLOAT = "float";
    static constexpr const char* PARAM_SHIMMER = "shimmer";
    static constexpr const char* PARAM_DLY_TIME = "dlyTime";
    static constexpr const char* PARAM_DLY_FB = "dlyFb";
    static constexpr const char* PARAM_DLY_MIX = "dlyMix";
    static constexpr const char* PARAM_REV_MIX = "revMix";
    static constexpr const char* PARAM_REV_SIZE = "revSize";

    // ¸ø Editor ¶ÁÈ¡µ±Ç°¸ùÒôÓÃ
    int getCurrentRoot() const noexcept { return currentRoot; }

private:
    //==============================================================================
    static constexpr int numVoices = 12;
    std::array<SineCloudVoice, numVoices> voices;

    // µ±Ç°¸ùÒô (0~11, C=0)
    int currentRoot{ 0 };

    // Csound °æÒô³Ø°ëÒôÆ«ÒÆ (giIntervals)
    static constexpr int kIntervals[12] = {
        0, 4, 7, 11, 14, 21, 12, 16, 19, 23, 26, 33
    };

    // Æ½»¬Æ÷£º¶ÔÓ¦ Csound µÄ portk
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowNoteSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dlyTimeSmooth;

    // Á¢ÌåÉùÑÓ³ÙÏß£¨×óÓÒ²»Í¬Ê±¼ä + ½»²æ·´À¡£©
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL{ 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR{ 192000 };
    float lastDelayOutL{ 0.0f };
    float lastDelayOutR{ 0.0f };

    // Delay ºó¶ËµÄµÍÍ¨£¨tone 4500Hz£©
    juce::dsp::FirstOrderTPTFilter<float> delayLpfL;
    juce::dsp::FirstOrderTPTFilter<float> delayLpfR;

    // Ö÷»ìÏì
    juce::dsp::Reverb reverb;

    // 3 Ãëµ­Èë
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> fadeIn;

    double currentSampleRate{ 44100.0 };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleMidiMessage(const juce::MidiMessage& msg);

    bool noteHeld{ false };
    int  heldNote{ -1 };


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineCloudAudioProcessor)
};
