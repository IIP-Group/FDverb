// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include <vector>

class EarlyReflections
{
public:
    EarlyReflections();
    ~EarlyReflections();

    void prepare(double sampleRate, int samplesPerBlock, int maxNumChannels, int maxNumTaps, float maxDelayMs);
    void reset();

    void calculateTaps(float predelayMs, float tailDelayMs, float decayTimeMs, float width, int numTaps);
    void processBlock(const juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer);

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> m_delayLine{44100};

    std::vector<std::vector<int>> m_tapDelays;      // [channel][tap] in samples
    std::vector<std::vector<float>> m_tapGains;     // [channel][tap]

    double m_sampleRate = 44100.0;
    int m_maxNumChannels = 2;
    int m_maxNumTaps = 32;
    int m_numTaps = 0;
    const float k_erDecayTimeScale = 0.16f;

    juce::Random m_random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EarlyReflections)
};
