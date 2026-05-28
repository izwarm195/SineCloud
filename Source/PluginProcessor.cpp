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
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
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
    return 1;
}

int SineCloudAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SineCloudAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String SineCloudAudioProcessor::getProgramName(int index)
{
    return {};
}

void SineCloudAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void SineCloudAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    for (auto& voice : voices)
        voice.prepareToPlay(sampleRate);
}

void SineCloudAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SineCloudAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

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

    // 处理 MIDI（按下切根音 + 开闸，松开关闸）
    for (const auto metadata : midiMessages)
        handleMidiMessage(metadata.getMessage());

    // 没有按键 → 不渲染，让所有 voice 安静
    if (!noteHeld)
    {
        for (auto& voice : voices)
            voice.silence();
        return;
    }

    // 从 APVTS 读所有参数
    float density = *apvts.getRawParameterValue(PARAM_DENSITY);
    const float pitch = *apvts.getRawParameterValue(PARAM_PITCH);
    const float attack = *apvts.getRawParameterValue(PARAM_ATTACK) / 1000.0f;
    const float sustain = *apvts.getRawParameterValue(PARAM_SUSTAIN) / 1000.0f;
    const float release = *apvts.getRawParameterValue(PARAM_RELEASE) / 1000.0f;
    const float decay = *apvts.getRawParameterValue(PARAM_DECAY) / 1000.0f;

    if (density < 0.05f) density = 0.05f;

    const float invDensity = 1.0f / density;
    float minGap = 0.15f * invDensity;
    float maxGap = 0.8f * invDensity;
    if (minGap < 0.01f) minGap = 0.01f;
    if (maxGap < minGap + 0.01f) maxGap = minGap + 0.01f;

    SineCloudVoice::VoiceParams params;
    params.lowNote = pitch;
    params.minGap = minGap;
    params.maxGap = maxGap;
    params.attackSec = attack;
    params.sustainSec = sustain;
    params.releaseSec = release;
    params.decaySec = decay;
    params.currentRoot = currentRoot;

    for (auto& voice : voices)
        voice.renderToBuffer(buffer, 0, buffer.getNumSamples(), params);
}

void SineCloudAudioProcessor::handleMidiMessage(const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        currentRoot = msg.getNoteNumber() % 12;
        heldNote = msg.getNoteNumber();
        noteHeld = true;
    }
    else if (msg.isNoteOff())
    {
        // 只有松开的是当前按住的那个音才关闸（防止按 A 再按 B 松 A 误关）
        if (msg.getNoteNumber() == heldNote)
        {
            noteHeld = false;
            heldNote = -1;
        }
    }
}

//==============================================================================
bool SineCloudAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SineCloudAudioProcessor::createEditor()
{
    return new SineCloudAudioProcessorEditor(*this);
}

//==============================================================================
void SineCloudAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SineCloudAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
SineCloudAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Pitch: 38 ~ 110 MIDI（音云的最低音锚点）
    params.push_back(std::make_unique<FloatParam>(
        juce::ParameterID{ PARAM_PITCH, 1 }, "Pitch",
        juce::NormalisableRange<float>(38.0f, 110.0f, 1.0f),
        62.0f));

    // Density: 0.2 ~ 32 Hz
    params.push_back(std::make_unique<FloatParam>(
        juce::ParameterID{ PARAM_DENSITY, 1 }, "Density",
        juce::NormalisableRange<float>(0.2f, 32.0f, 0.0f, 0.4f),
        4.0f));

    // ADSR (毫秒)
    params.push_back(std::make_unique<FloatParam>(
        juce::ParameterID{ PARAM_ATTACK, 1 }, "Attack",
        juce::NormalisableRange<float>(0.0f, 3000.0f, 1.0f, 0.5f),
        1000.0f));
    params.push_back(std::make_unique<FloatParam>(
        juce::ParameterID{ PARAM_SUSTAIN, 1 }, "Sustain",
        juce::NormalisableRange<float>(0.0f, 5000.0f, 1.0f, 0.5f),
        500.0f));
    params.push_back(std::make_unique<FloatParam>(
        juce::ParameterID{ PARAM_RELEASE, 1 }, "Release",
        juce::NormalisableRange<float>(0.0f, 3000.0f, 1.0f, 0.5f),
        1000.0f));
    params.push_back(std::make_unique<FloatParam>(
        juce::ParameterID{ PARAM_DECAY, 1 }, "Decay",
        juce::NormalisableRange<float>(0.0f, 5000.0f, 1.0f, 0.5f),
        0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SineCloudAudioProcessor();
}
