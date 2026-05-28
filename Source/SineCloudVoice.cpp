#include "SineCloudVoice.h"

void SineCloudVoice::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate;
    state = State::Waiting;
    waitTimer = 0.0;
    // 错开各 voice 初始 wait，避免 12 voice 同时触发
    waitDuration = random.nextDouble() * 0.8 + 0.2;
}

void SineCloudVoice::triggerNewParticle(const VoiceParams& params)
{
    // === 音池：12 个偏移量（Csound giIntervals）===
    static constexpr int kIntervals[12] = {
        0, 4, 7, 11, 14, 21, 12, 16, 19, 23, 26, 33
    };

    const int idx = random.nextInt(12);
    int midi = 36 + params.currentRoot + kIntervals[idx];

    // 八度包裹到 [lowNote, lowNote + 24]
    const int low = juce::roundToInt(params.lowNote);
    while (midi < low)        midi += 12;
    while (midi > low + 24)   midi -= 12;

    targetMidi = (double)midi;
    startMidi = targetMidi;     // 暂时不用 Shimmer / Float
    currentMidi = startMidi;

    curAttack = juce::jmax(0.001, (double)params.attackSec);
    curSustain = juce::jmax(0.0, (double)params.sustainSec);
    curRelease = juce::jmax(0.001, (double)params.releaseSec);
    curDecay = juce::jmax(0.001, (double)params.decaySec);
    hasDecay = (params.decaySec >= 0.001);

    // sustain 抖动 0~0.4 秒
    const double susJitter = random.nextDouble() * 0.4;

    if (hasDecay)
        duration = curAttack + curSustain + susJitter + curRelease + curDecay;
    else
        duration = curAttack + curSustain + susJitter + curRelease;

    amplitude = 0.05 + random.nextDouble() * 0.07;  // 0.05 ~ 0.12

    phase = 0.0;
    currentAngle = 0.0;

    // 计算频率 / angleDelta
    const double freqHz = 440.0 * std::pow(2.0, (currentMidi - 69.0) / 12.0);
    angleDelta = (freqHz / currentSampleRate) * juce::MathConstants<double>::twoPi;

    state = State::Playing;
}

double SineCloudVoice::calculateEnvelope() const
{
    if (state != State::Playing) return 0.0;

    const double T1 = curAttack;
    const double T2 = T1 + curSustain;       // 注意：实际还有 jitter，但用近似
    const double T3 = T2 + curRelease;

    const double dcyFloor = hasDecay ? 0.3 : 0.0;

    double env = 0.0;
    if (phase < T1)
    {
        env = phase / curAttack;
    }
    else if (phase < T2)
    {
        env = 1.0;
    }
    else if (phase < T3)
    {
        const double rel = (phase - T2) / curRelease;
        env = 1.0 - rel * (1.0 - dcyFloor);
    }
    else
    {
        const double dec = (phase - T3) / curDecay;
        env = dcyFloor * std::exp(-4.0 * dec);
    }

    return juce::jlimit(0.0, 1.0, env);
}

void SineCloudVoice::renderToBuffer(juce::AudioBuffer<float>& outputBuffer,
    int startSample,
    int numSamples,
    const VoiceParams& params)
{
    const double dt = 1.0 / currentSampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (state == State::Waiting)
        {
            waitTimer += dt;
            if (waitTimer >= waitDuration)
            {
                waitTimer = 0.0;
                triggerNewParticle(params);
            }
            // 写 0 到 buffer (实际不写，因为 buffer 已 clear)
        }
        else // Playing
        {
            // 推进 phase
            phase += dt;

            if (phase >= duration)
            {
                state = State::Waiting;
                // 随机下一次等待时间
                const double range = juce::jmax(0.001f, params.maxGap - params.minGap);
                waitDuration = params.minGap + random.nextDouble() * range;
                waitTimer = 0.0;
                continue;
            }

            // 包络
            const double env = calculateEnvelope();

            // 振荡器
            const double sineValue = std::sin(currentAngle);
            const double out = sineValue * amplitude * env;

            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                outputBuffer.addSample(channel, startSample + sample, (float)out);

            currentAngle += angleDelta;
            if (currentAngle >= juce::MathConstants<double>::twoPi)
                currentAngle -= juce::MathConstants<double>::twoPi;
        }
    }
}
