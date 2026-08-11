// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include <vector>

class EarlyReflections
{
public:
    static constexpr int kMaxNumTaps = 32;

    struct Tap
    {
        int   delaySamples = 0;
        float gainL = 0.0f;
        float gainR = 0.0f;   // zero when numChannels < 2
    };

    EarlyReflections();
    ~EarlyReflections();

    void prepare(double sampleRate, int samplesPerBlock, int maxNumChannels, int maxNumTaps, float maxDelayMs);
    void reset();

    // Equal pre-delay and tail delay would collapse every tap onto one delay, so the two are kept
    // 1 ms apart. The editor enforces this on the knobs, but automation and restored sessions do not.
    static float clampPreDelayMs(float preDelayMs, float tailDelayMs);

    // Builds the tap table and returns the number of taps written to out, which must have room for
    // kMaxNumTaps entries. Pure and deterministic, so the editor can plot exactly what is audible.
    static int computeTaps(float preDelayMs, float tailDelayMs, float width,
                           int numTaps, double sampleRate, int numChannels, Tap* out);

    void calculateTaps(float predelayMs, float tailDelayMs, float width, int numTaps);
    // numSamples must be the current host block length, not the allocated buffer length
    void processBlock(const juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer, int numSamples);

private:
    // tap delays are whole samples, so interpolation would always return the sample itself
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> m_delayLine{44100};

    std::vector<std::vector<int>> m_tapDelays;      // [channel][tap] in samples
    std::vector<std::vector<float>> m_tapGains;     // [channel][tap]

    double m_sampleRate = 44100.0;
    int m_maxNumChannels = 2;
    int m_maxNumTaps = 32;
    int m_numTaps = 0;
    // Taps decay at this fixed rate from the first tap onwards, independent of the reverb decay.
    // 0.2 dB/ms reaches -60 dB 300 ms after the first tap.
    static constexpr float k_erDecayDbPerMs = 0.2f;

    // the tap table only changes with these parameters, so it is not recomputed per block
    bool m_tapsValid = false;
    float m_lastPreDelayMs = 0.0f;
    float m_lastTailDelayMs = 0.0f;
    float m_lastWidth = 0.0f;
    int m_lastNumTaps = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EarlyReflections)
};
