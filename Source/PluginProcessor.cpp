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

SineCloudAudioProcessor::~SineCloudAudioProcessor() {}

//==============================================================================
const juce::String SineCloudAudioProcessor::getName() const { return JucePlugin_Name; }
bool   SineCloudAudioProcessor::acceptsMidi()   const { return true; }
bool   SineCloudAudioProcessor::producesMidi()  const { return false; }
bool   SineCloudAudioProcessor::isMidiEffect()  const { return false; }
double SineCloudAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int    SineCloudAudioProcessor::getNumPrograms() { return 1; }
int    SineCloudAudioProcessor::getCurrentProgram() { return 0; }
void   SineCloudAudioProcessor::setCurrentProgram(int) {}
const  juce::String SineCloudAudioProcessor::getProgramName(int) { return {}; }
void   SineCloudAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void SineCloudAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 1;

    // 延迟线（最大 2s）
    const int maxDelaySamples = (int)(sampleRate * 2.0) + 16;
    delayL.reset();
    delayR.reset();
    delayL.setMaximumDelayInSamples(maxDelaySamples);
    delayR.setMaximumDelayInSamples(maxDelaySamples);
    delayL.prepare(spec);
    delayR.prepare(spec);
    lastDelayOutL = lastDelayOutR = 0.0f;

    // Delay LPF (tone 4500Hz)
    delayLpfL.prepare(spec);
    delayLpfR.prepare(spec);
    delayLpfL.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    delayLpfR.setType(juce::dsp::FirstOrderTPTFilterType::lowpass);
    delayLpfL.setCutoffFrequency(4500.0f);
    delayLpfR.setCutoffFrequency(4500.0f);

    // Reverb
    juce::dsp::ProcessSpec rvSpec;
    rvSpec.sampleRate = sampleRate;
    rvSpec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    rvSpec.numChannels = 2;
    reverb.prepare(rvSpec);

    // 平滑器 (Csound: portk lowNote 0.4s, dlyTime 0.1s)
    lowNoteSmooth.reset(sampleRate, 0.4);
    dlyTimeSmooth.reset(sampleRate, 0.1);
    lowNoteSmooth.setCurrentAndTargetValue(*apvts.getRawParameterValue(PARAM_PITCH));
    dlyTimeSmooth.setCurrentAndTargetValue(*apvts.getRawParameterValue(PARAM_DLY_TIME));

    // 3 秒淡入
    fadeIn.reset(sampleRate, 3.0);
    fadeIn.setCurrentAndTargetValue(0.0f);
    fadeIn.setTargetValue(1.0f);

    // 给每个 Voice 不同的初始等待时间，错开触发
    juce::Random rng;
    for (int i = 0; i < numVoices; ++i)
    {
        voices[i].prepareToPlay(sampleRate);
        voices[i].setInitialWait(0.05 + rng.nextDouble() * 1.5);
    }

}

void SineCloudAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SineCloudAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
}
#endif

//==============================================================================
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
        if (msg.getNoteNumber() == heldNote)
        {
            noteHeld = false;
            heldNote = -1;
        }
    }
}


//==============================================================================
void SineCloudAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples <= 0)
        return;

    // ---- MIDI ----
    for (const auto m : midiMessages)
        handleMidiMessage(m.getMessage());

    // ---- 读参数 ----
    float pitch = *apvts.getRawParameterValue(PARAM_PITCH);
    float density = *apvts.getRawParameterValue(PARAM_DENSITY);
    const float atkMs = *apvts.getRawParameterValue(PARAM_ATTACK);
    const float decMs = *apvts.getRawParameterValue(PARAM_DECAY);
    const float susMs = *apvts.getRawParameterValue(PARAM_SUSTAIN);
    const float relMs = *apvts.getRawParameterValue(PARAM_RELEASE);
    const float gain = *apvts.getRawParameterValue(PARAM_GAIN);
    const float dream = *apvts.getRawParameterValue(PARAM_DREAM);
    const float floatMs = *apvts.getRawParameterValue(PARAM_FLOAT);
    const float shimmer = *apvts.getRawParameterValue(PARAM_SHIMMER);
    const float dlyTime = *apvts.getRawParameterValue(PARAM_DLY_TIME);
    const float dlyFb = *apvts.getRawParameterValue(PARAM_DLY_FB);
    const float dlyMix = *apvts.getRawParameterValue(PARAM_DLY_MIX);
    const float revMix = *apvts.getRawParameterValue(PARAM_REV_MIX);
    const float revSize = juce::jlimit(0.7f, 0.99f,
        (float)*apvts.getRawParameterValue(PARAM_REV_SIZE));

    if (density < 0.05f) density = 0.05f;
    if (pitch < 38.0f) pitch = 38.0f;

    // Csound: kLowQuant = int(kLow+0.5); kLowSmooth portk kLowQuant, 0.4
    lowNoteSmooth.setTargetValue(std::round(pitch));
    dlyTimeSmooth.setTargetValue(dlyTime);

    // minGap / maxGap
    const float inv = 1.0f / density;
    float minGap = juce::jmax(0.01f, 0.15f * inv);
    float maxGap = 0.8f * inv;
    if (maxGap < minGap + 0.01f) maxGap = minGap + 0.01f;

    // 组装 voice 参数
    SineCloudVoice::VoiceParams vp;
    vp.lowNote = lowNoteSmooth.getNextValue();  // 取一次（块内当常量）
    vp.minGap = minGap;
    vp.maxGap = maxGap;
    vp.attackSec = juce::jmax(0.001f, atkMs / 1000.0f);
    vp.sustainSec = juce::jmax(0.0f, susMs / 1000.0f);
    vp.releaseSec = juce::jmax(0.001f, relMs / 1000.0f);
    vp.decaySec = juce::jmax(0.0f, decMs / 1000.0f);
    vp.dream = juce::jlimit(0.0f, 12.0f, dream);
    vp.floatMs = juce::jlimit(10.0f, 100.0f, floatMs);
    vp.shimmer = juce::jlimit(0.0f, 48.0f, shimmer);
    vp.currentRoot = currentRoot;
    vp.intervals = kIntervals;
    vp.gateOpen = noteHeld;   // <-- 新增


    // 让平滑器跑完剩下的样本数（保持步进）
    for (int i = 1; i < numSamples; ++i) { lowNoteSmooth.getNextValue(); dlyTimeSmooth.getNextValue(); }

    // ---- 渲染 12 voice 到 dry buffer ----
    for (auto& v : voices)
        v.renderToBuffer(buffer, 0, numSamples, vp);

    // ---- 延迟 + 反馈（交叉）+ tone 低通 ----
    const float sr = (float)currentSampleRate;
    const float dlySmpL = juce::jlimit(1.0f, sr * 2.0f, dlyTime * 0.001f * sr);
    const float dlySmpR = juce::jlimit(1.0f, sr * 2.0f, dlyTime * 1.27f * 0.001f * sr);
    delayL.setDelay(dlySmpL);
    delayR.setDelay(dlySmpR);

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    // 先取一份 dry（用于后面叠加）
    juce::AudioBuffer<float> dryCopy;
    dryCopy.makeCopyOf(buffer);
    const float* dryL = dryCopy.getReadPointer(0);
    const float* dryR = dryCopy.getReadPointer(1);

    // dly buffer
    juce::AudioBuffer<float> dlyBuf(2, numSamples);
    dlyBuf.clear();
    float* dL = dlyBuf.getWritePointer(0);
    float* dR = dlyBuf.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        // Csound: aDlyInL = aDryL + aDlyFbR * kDlyFb  (交叉)
        const float inL = dryL[i] + lastDelayOutR * dlyFb;
        const float inR = dryR[i] + lastDelayOutL * dlyFb;

        delayL.pushSample(0, inL);
        delayR.pushSample(0, inR);

        float outL = delayL.popSample(0);
        float outR = delayR.popSample(0);

        // tone 4500Hz
        outL = delayLpfL.processSample(0, outL);
        outR = delayLpfR.processSample(0, outR);

        lastDelayOutL = outL;
        lastDelayOutR = outR;

        dL[i] = outL;
        dR[i] = outR;
    }

    // ---- Reverb 输入 = dry + dly*0.5 ----
    juce::AudioBuffer<float> revBuf(2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        revBuf.setSample(0, i, dryL[i] + dL[i] * 0.5f);
        revBuf.setSample(1, i, dryR[i] + dR[i] * 0.5f);
    }

    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = juce::jmap(revSize, 0.7f, 0.99f, 0.5f, 0.95f);
    rp.damping = 0.3f;
    rp.wetLevel = 1.0f;  // 全 wet（混合在外面做）
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters(rp);

    juce::dsp::AudioBlock<float> revBlock(revBuf);
    juce::dsp::ProcessContextReplacing<float> revCtx(revBlock);
    reverb.process(revCtx);

    const float* rL = revBuf.getReadPointer(0);
    const float* rR = revBuf.getReadPointer(1);



    // 简化：用线性插值的 fade
    // ---- 混合输出 ----
    for (int i = 0; i < numSamples; ++i)
    {
        const float fd = fadeIn.getNextValue();   // 每样本推进，0 → 1（3 秒）

        L[i] = (dryL[i] + dL[i] * dlyMix + rL[i] * revMix) * fd * gain * 0.6f;
        R[i] = (dryR[i] + dR[i] * dlyMix + rR[i] * revMix) * fd * gain * 0.6f;
    }
}


//==============================================================================
bool SineCloudAudioProcessor::hasEditor() const { return true; }
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
    using FP = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto add = [&](const char* id, const juce::String& name,
        float min, float max, float def, float skew = 1.0f)
        {
            p.push_back(std::make_unique<FP>(
                juce::ParameterID{ id, 1 }, name,
                juce::NormalisableRange<float>(min, max, 0.0f, skew), def));
        };

    add(PARAM_PITCH, "Pitch", 38.0f, 110.0f, 62.0f);
    add(PARAM_DENSITY, "Density", 0.2f, 32.0f, 1.0f, 0.5f);
    add(PARAM_ATTACK, "Attack", 0.0f, 3000.0f, 1000.0f, 0.5f);
    add(PARAM_DECAY, "Decay", 0.0f, 5000.0f, 0.0f, 0.5f);
    add(PARAM_SUSTAIN, "Sustain", 0.0f, 5000.0f, 500.0f, 0.5f);
    add(PARAM_RELEASE, "Release", 0.0f, 3000.0f, 1000.0f, 0.5f);
    add(PARAM_GAIN, "Gain", 0.0f, 1.0f, 0.6f);
    add(PARAM_DREAM, "Dream", 0.0f, 12.0f, 0.0f);
    add(PARAM_FLOAT, "Float", 10.0f, 100.0f, 50.0f);
    add(PARAM_SHIMMER, "Shimmer", 0.0f, 48.0f, 0.0f);
    add(PARAM_DLY_TIME, "Dly Time", 50.0f, 2000.0f, 700.0f, 0.5f);
    add(PARAM_DLY_FB, "Dly Fb", 0.0f, 0.95f, 0.55f);
    add(PARAM_DLY_MIX, "Dly Mix", 0.0f, 1.0f, 0.35f);
    add(PARAM_REV_MIX, "Rev Mix", 0.0f, 1.0f, 0.55f);
    add(PARAM_REV_SIZE, "Rev Size", 0.7f, 0.99f, 0.92f);

    return { p.begin(), p.end() };
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SineCloudAudioProcessor();
}
