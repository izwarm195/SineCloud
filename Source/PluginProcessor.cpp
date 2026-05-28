/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"



//==============================================================================
SineCloudAudioProcessor::SineCloudAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    // 全局 ADSR 默认参数（Sine Cloud ambient 风格）
    adsrParams.attack = 0.5f;    // 500ms 慢起
    adsrParams.decay = 0.2f;
    adsrParams.sustain = 0.8f;
    adsrParams.release = 2.0f;    // 2 秒慢消
    adsr.setParameters(adsrParams);
}




SineCloudAudioProcessor::~SineCloudAudioProcessor()
{


}

//==============================================================================
const juce::String SineCloudAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SineCloudAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SineCloudAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SineCloudAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SineCloudAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SineCloudAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SineCloudAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SineCloudAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SineCloudAudioProcessor::getProgramName (int index)
{
    return {};
}

void SineCloudAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SineCloudAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    for (auto& voice : voices)
        voice.prepareToPlay(sampleRate);

    adsr.setSampleRate(sampleRate);
}



void SineCloudAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SineCloudAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif



void SineCloudAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // === 处理 MIDI 消息 ===
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        handleMidiMessage(msg);
    }

    // === 渲染所有 voice ===
    for (auto& voice : voices)
        voice.renderToBuffer(buffer, 0, buffer.getNumSamples());

    // === 整体施加 ADSR ===
    const int numSamples = buffer.getNumSamples();
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float envValue = adsr.getNextSample();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            data[sample] *= envValue;
        }
    }
}


//==============================================================================
bool SineCloudAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SineCloudAudioProcessor::createEditor()
{
    return new SineCloudAudioProcessorEditor (*this);
}

//==============================================================================
void SineCloudAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SineCloudAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SineCloudAudioProcessor();
}

void SineCloudAudioProcessor::handleMidiMessage(const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        triggerNoteOn(msg.getNoteNumber());
    }
    else if (msg.isNoteOff())
    {
        // 只有当松开的是当前根音才 release
        // （多键按下时只追踪最新一次按键）
        if (msg.getNoteNumber() == currentRootMidi)
            triggerNoteOff();
    }
}

void SineCloudAudioProcessor::triggerNoteOn(int rootMidi)
{
    currentRootMidi = rootMidi;
    isNoteHeld = true;

    // 给每个 voice 设定音高
    for (int i = 0; i < numVoices; ++i)
    {
        const int targetMidi = rootMidi + kIntervals[i];
        voices[i].setMidiNote((double)targetMidi);
        voices[i].setLevel(0.1);   // 12 voice 叠加，每个 0.1 防爆音
        voices[i].resetPhase();
    }

    // 触发全局 ADSR
    adsr.reset();
    adsr.noteOn();
}

void SineCloudAudioProcessor::triggerNoteOff()
{
    isNoteHeld = false;
    adsr.noteOff();
    // voice 不立即停，让 release 阶段还能听到
}

