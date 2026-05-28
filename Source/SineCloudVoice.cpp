#include "SineCloudVoice.h"

//==============================================================================
void SineCloudVoice::prepareToPlay(double sr)
{
    sampleRate = sr;
    state = State::Waiting;
    waitElapsed = 0.0;
    waitDuration = 0.5;
    phase = 0.0;
    angleBaseUp = 0.0;
    angleBaseDn = 0.0;
    angleL1Up = 0.0;
    angleL1Dn = 0.0;
}

void SineCloudVoice::setInitialWait(double seconds)
{
    waitDuration = seconds;
    waitElapsed = 0.0;
}

//==============================================================================
void SineCloudVoice::triggerNewParticle(const VoiceParams& p)
{
    // 1) 从音池随机抽一个半音偏移
    const int idx = random.nextInt(12);
    const int interval = (p.intervals != nullptr) ? p.intervals[idx] : 0;

    // 2) 起始 MIDI = 36 + root + interval，再八度包裹到 [lowNote, lowNote+24]
    double midi = 36.0 + (double)p.currentRoot + (double)interval;
    while (midi < (double)p.lowNote)         midi += 12.0;
    while (midi > (double)p.lowNote + 24.0)  midi -= 12.0;

    targetMidi = midi;
    startMidi = midi + (double)p.shimmer;   // Shimmer：起音高，滑向目标
    curEnvTime = juce::jmax(0.001, (double)p.floatMs / 1000.0);

    // 3) Dream 控制 Pan 摆幅
    curDream = (double)p.dream;
    const double dN = curDream / 12.0;
    const double pan = dN * 0.5;
    if (random.nextFloat() < 0.5f) { panStart = 0.5 - pan; panEnd = 0.5 + pan; }
    else { panStart = 0.5 + pan; panEnd = 0.5 - pan; }

    // 4) ADSR
    curAtk = juce::jmax(0.001, (double)p.attackSec);
    curSus = juce::jmax(0.0, (double)p.sustainSec);
    curRel = juce::jmax(0.001, (double)p.releaseSec);
    curDcy = juce::jmax(0.001, (double)p.decaySec);
    hasDcy = (p.decaySec >= 0.001f);

    susJitter = random.nextDouble() * 0.4;

    duration = curAtk + curSus + susJitter + curRel + (hasDcy ? curDcy : 0.0);

    // 5) 振幅 0.05 ~ 0.12
    amplitude = 0.05 + random.nextDouble() * 0.07;

    phase = 0.0;
    state = State::Playing;
}

//==============================================================================
void SineCloudVoice::renderToBuffer(juce::AudioBuffer<float>& buffer,
    int startSample,
    int numSamples,
    const VoiceParams& p)
{
    if (buffer.getNumChannels() < 2 || numSamples <= 0)
        return;

    float* outL = buffer.getWritePointer(0, startSample);
    float* outR = buffer.getWritePointer(1, startSample);

    const double dt = 1.0 / sampleRate;

    for (int n = 0; n < numSamples; ++n)
    {
        // ---------- 状态机 ----------
        if (state == State::Waiting)
        {
            // 关闸：不推进等待计时器，也不触发新粒子，直接静音
            if (!p.gateOpen)
                continue;

            waitElapsed += dt;
            if (waitElapsed >= waitDuration)
            {
                waitElapsed = 0.0;
                triggerNewParticle(p);
            }
            else
            {
                continue;
            }
        }


        // ---------- Playing ----------
        // 当前 MIDI（前 curEnvTime 秒从 startMidi 线性滑到 targetMidi = Shimmer 下滑）
        double curMidi;
        if (phase < curEnvTime)
        {
            const double prog = phase / curEnvTime;
            curMidi = startMidi + (targetMidi - startMidi) * prog;
        }
        else
        {
            curMidi = targetMidi;
        }

        // 当前 Pan（同样在 curEnvTime 内从 panStart 滑到 panEnd）
        double pan;
        if (phase < curEnvTime)
        {
            const double prog = phase / curEnvTime;
            pan = panStart + (panEnd - panStart) * prog;
        }
        else
        {
            pan = panEnd;
        }

        // 4 段包络
        const double t1 = curAtk;
        const double t2 = t1 + curSus + susJitter;
        const double t3 = t2 + curRel;
        const double dcyFloor = hasDcy ? 0.3 : 0.0;

        double env;
        if (phase < t1)
        {
            env = phase / curAtk;
        }
        else if (phase < t2)
        {
            env = 1.0;
        }
        else if (phase < t3)
        {
            const double relProg = (phase - t2) / curRel;
            env = 1.0 - relProg * (1.0 - dcyFloor);
        }
        else
        {
            const double dcyProg = (phase - t3) / curDcy;
            env = dcyFloor * std::exp(-4.0 * dcyProg);
        }
        env = juce::jlimit(0.0, 1.0, env);

        // Dream：基础层 + 上方四度层（+5 半音），都做 up/down 失谐
        const double dN = curDream / 12.0;
        const double stackW1 = 0.7 * dN;
        const double detCents = dN * 12.0;
        const double detLn = (detCents / 1200.0) * std::log(2.0);
        const double rUp = std::exp(detLn);
        const double rDn = std::exp(-detLn);

        const double freq0 = 440.0 * std::pow(2.0, (curMidi - 69.0) / 12.0);
        const double freq1 = 440.0 * std::pow(2.0, (curMidi + 5.0 - 69.0) / 12.0);

        // 基础层
        const double sBaseUp = std::sin(angleBaseUp);
        const double sBaseDn = std::sin(angleBaseDn);
        const double layer0 = (sBaseUp + sBaseDn) * 0.5;

        // 四度层
        const double sL1Up = std::sin(angleL1Up);
        const double sL1Dn = std::sin(angleL1Dn);
        const double layer1 = (sL1Up + sL1Dn) * 0.5 * stackW1;

        const double norm = 1.0 / (1.0 + stackW1);
        double mono = (layer0 + layer1) * norm * amplitude * env;

        // Pan (等功率)
        const double panC = juce::jlimit(0.0, 1.0, pan);
        const double gL = std::sqrt(1.0 - panC);
        const double gR = std::sqrt(panC);

        outL[n] += (float)(mono * gL);
        outR[n] += (float)(mono * gR);

        // 更新相位
        const double twoPi = juce::MathConstants<double>::twoPi;
        angleBaseUp += twoPi * freq0 * rUp * dt;
        angleBaseDn += twoPi * freq0 * rDn * dt;
        angleL1Up += twoPi * freq1 * rUp * dt;
        angleL1Dn += twoPi * freq1 * rDn * dt;

        if (angleBaseUp > twoPi) angleBaseUp -= twoPi;
        if (angleBaseDn > twoPi) angleBaseDn -= twoPi;
        if (angleL1Up > twoPi) angleL1Up -= twoPi;
        if (angleL1Dn > twoPi) angleL1Dn -= twoPi;

        // 推进
        phase += dt;
        if (phase >= duration)
        {
            state = State::Waiting;
            waitDuration = juce::jlimit(0.01,
                10.0,
                (double)p.minGap + random.nextDouble() * (p.maxGap - p.minGap));
            waitElapsed = 0.0;
        }
    }
}
