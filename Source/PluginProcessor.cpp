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
    currentSampleRate = sampleRate;

    for (auto& voice : voices)
        voice.prepareToPlay(sampleRate);

    adsr.setSampleRate(sampleRate);

    // 初始化触发间隔
    samplesPerTrigger = sampleRate / density;
    samplesUntilNextTrigger = 0.0;
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

    // 处理 MIDI
    for (const auto metadata : midiMessages)
        handleMidiMessage(metadata.getMessage());

    const int numSamples = buffer.getNumSamples();

    // 更新触发间隔（density 可能在运行中变化）
    samplesPerTrigger = currentSampleRate / juce::jmax(0.2f, density);

    // === 在 block 内推进触发计时器 ===
    // 只在 ADSR 还活跃时触发新粒子
    if (adsr.isActive())
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            samplesUntilNextTrigger -= 1.0;
            if (samplesUntilNextTrigger <= 0.0)
            {
                triggerParticles(sample);
                samplesUntilNextTrigger += samplesPerTrigger;
            }
        }
    }

    // === 渲染所有 voice ===
    for (auto& voice : voices)
        voice.renderToBuffer(buffer, 0, numSamples);

    // === 整体 ADSR ===
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
        currentRootMidi = msg.getNoteNumber();
        adsr.reset();
        adsr.noteOn();
        // 立即触发第一颗粒子（不等下一次 metro tick）
        samplesUntilNextTrigger = 0.0;
    }
    else if (msg.isNoteOff() && msg.getNoteNumber() == currentRootMidi)
    {
        adsr.noteOff();
    }
}

void SineCloudAudioProcessor::triggerParticles(int /*blockOffset*/)
{
    if (currentRootMidi < 0)
        return;

    // 计算实际根音（含 Pitch 旋钮偏移）
    const int actualRoot = currentRootMidi + (int)std::round(pitchOffset);

    // 随机选一个 voice
    const int voiceIndex = random.nextInt(numVoices);

    // 随机从音池抽一个音
    const int intervalIndex = random.nextInt(numVoices);
    const double targetMidi = (double)(actualRoot + kIntervals[intervalIndex]);

    // 触发
    voices[voiceIndex].triggerParticle(targetMidi, 0.3);
}
/*
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
*/
