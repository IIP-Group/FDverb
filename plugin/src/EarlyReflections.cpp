// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <algorithm>

#include "EarlyReflections.h"

EarlyReflections::EarlyReflections()
{
}

EarlyReflections::~EarlyReflections()
{
}

void EarlyReflections::prepare(double sampleRate, int samplesPerBlock, int maxNumChannels, int maxNumTaps, float maxDelayMs)
{
    m_sampleRate = sampleRate;
    m_maxNumChannels = maxNumChannels;
    m_maxNumTaps = maxNumTaps;
    
    int maxDelayInSamples = static_cast<int>(maxDelayMs * 0.001f * sampleRate);
    m_delayLine.setMaximumDelayInSamples(maxDelayInSamples);
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels = static_cast<uint32_t>(maxNumChannels);
    m_delayLine.prepare(spec);
    
    m_tapDelays.resize(static_cast<size_t>(maxNumChannels));
    m_tapGains.resize(static_cast<size_t>(maxNumChannels));
    
    for (size_t ch = 0; ch < static_cast<size_t>(maxNumChannels); ++ch) {
        m_tapDelays[ch].resize(static_cast<size_t>(maxNumTaps), 0);
        m_tapGains[ch].resize(static_cast<size_t>(maxNumTaps), 0);
    }

    // tap delays depend on the sample rate, so force a recompute on the next block
    m_tapsValid = false;

    reset();
}

void EarlyReflections::reset()
{
    m_delayLine.reset();
}

float EarlyReflections::clampPreDelayMs(float preDelayMs, float tailDelayMs)
{
    return std::min(preDelayMs, std::max(0.0f, tailDelayMs - 1.0f));
}

int EarlyReflections::computeTaps(float preDelayMs, float tailDelayMs, float width,
                                 int numTaps, double sampleRate, int numChannels, Tap* out)
{
    numTaps = std::clamp(numTaps, 0, kMaxNumTaps);
    if (numTaps == 0) {
        return 0;
    }

    // The editor keeps pre-delay below tail delay, but automation and restored sessions bypass it.
    preDelayMs = clampPreDelayMs(preDelayMs, tailDelayMs);

    const float tailDelaySamples = tailDelayMs * 0.001 * sampleRate;
    const float uniformStart = std::powf(preDelayMs / tailDelayMs, 3); // ^3 compensates cubic root

    juce::Random random;
    random.setSeed(static_cast<juce::int64>(preDelayMs));
    random.nextInt(); // warm up: the draws below depend on the seed this advances

    for (int i = 0; i < numTaps; ++i) {
        // calculate tap position
        float r; // random variable
        if (i == 0) { // enforce first tap at pre-delay
            r = uniformStart;
        }
        else {
            // r uniform in [uniformStart, 1]
            r = random.nextFloat();
            r = r * (1.f - uniformStart) + uniformStart;
        }
        r = std::cbrtf(r);

        int delaySamples = int(r * tailDelaySamples);
        delaySamples = std::max(1, delaySamples);

        // calculate amplitude with decay, measured from the first tap rather than from the dry
        // signal, so the first tap always lands at unity
        const float tapMs = r * tailDelayMs;
        float gain = std::powf(10, -0.05f * k_erDecayDbPerMs * (tapMs - preDelayMs));

        // calculate stereo pan
        float pan = 0.5f + width * (random.nextFloat() - 0.5f);
        pan = std::clamp(pan, 0.0f, 1.0f);
        if (numChannels == 1) {
            pan = 0.5f;
        }

        out[i].delaySamples = delaySamples;
        out[i].gainL = gain * std::sqrt(1.0f - pan);
        out[i].gainR = (numChannels >= 2) ? gain * std::sqrt(pan) : 0.0f;
    }

    return numTaps;
}

void EarlyReflections::calculateTaps(float preDelayMs, float tailDelayMs, float width, int numTaps)
{
    if (m_tapsValid
        && preDelayMs == m_lastPreDelayMs
        && tailDelayMs == m_lastTailDelayMs
        && width == m_lastWidth
        && numTaps == m_lastNumTaps) {
        return;
    }

    m_lastPreDelayMs = preDelayMs;
    m_lastTailDelayMs = tailDelayMs;
    m_lastWidth = width;
    m_lastNumTaps = numTaps;
    m_tapsValid = true;

    for (size_t ch = 0; ch < static_cast<size_t>(m_maxNumChannels); ++ch) {
        std::fill(m_tapDelays[ch].begin(), m_tapDelays[ch].end(), 0);
        std::fill(m_tapGains[ch].begin(), m_tapGains[ch].end(), 0.0f);
    }

    Tap taps[kMaxNumTaps];
    m_numTaps = computeTaps(preDelayMs, tailDelayMs, width,
                            std::min(m_maxNumTaps, numTaps), m_sampleRate, m_maxNumChannels, taps);

    for (int i = 0; i < m_numTaps; ++i) {
        m_tapDelays[0][static_cast<size_t>(i)] = taps[i].delaySamples;
        m_tapGains[0][static_cast<size_t>(i)] = taps[i].gainL;

        if (m_maxNumChannels == 2) {
            m_tapDelays[1][static_cast<size_t>(i)] = taps[i].delaySamples;
            m_tapGains[1][static_cast<size_t>(i)] = taps[i].gainR;
        }
    }
}

void EarlyReflections::processBlock(const juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer, int numSamples)
{
    numSamples = std::min({numSamples, dryBuffer.getNumSamples(), wetBuffer.getNumSamples()});

    int numChannels = std::min({dryBuffer.getNumChannels(), wetBuffer.getNumChannels(),
                                m_maxNumChannels, static_cast<int>(m_tapDelays.size())});

    for (int ch = 0; ch < numChannels; ++ch) {
        const float* dryData = dryBuffer.getReadPointer(ch);
        float* wetData = wetBuffer.getWritePointer(ch);
        const int* tapDelays = m_tapDelays[static_cast<size_t>(ch)].data();
        const float* tapGains = m_tapGains[static_cast<size_t>(ch)].data();

        for (int i = 0; i < numSamples; ++i) {
            // push dry sample into delay line
            m_delayLine.pushSample(ch, dryData[i]);

            // sum all taps, leaving the read pointer in place
            float sum = 0.0f;
            for (int tap = 0; tap < m_numTaps; ++tap) {
                sum += m_delayLine.popSample(ch, static_cast<float>(tapDelays[tap]), false) * tapGains[tap];
            }
            wetData[i] = sum;

            // advance read pointer exactly once per sample, also when there are no taps
            m_delayLine.popSample(ch, -1.0f, true);
        }
    }
}
