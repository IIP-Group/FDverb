// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    
    reset();
}

void EarlyReflections::reset()
{
    m_delayLine.reset();
}

void EarlyReflections::calculateTaps(float preDelayMs, float tailDelayMs, float decayTimeMs, float width, int numTaps)
{
    m_numTaps = std::min(m_maxNumTaps, numTaps);
    
    for (size_t ch = 0; ch < static_cast<size_t>(m_maxNumChannels); ++ch) {
        std::fill(m_tapDelays[ch].begin(), m_tapDelays[ch].end(), 0);
        std::fill(m_tapGains[ch].begin(), m_tapGains[ch].end(), 0.0f);
    }
    
    if (m_numTaps == 0) {
        return;
    }
    
    jassert (preDelayMs <= tailDelayMs);
    const float tailDelaySamples = tailDelayMs * 0.001 * m_sampleRate;
    const float uniformStart = std::powf(preDelayMs / tailDelayMs, 3); // ^3 compensates cubic root
    const float erDecayTimeMs = decayTimeMs * k_erDecayTimeScale;

    m_random.setSeed(static_cast<juce::int64>(preDelayMs));
    volatile int sink = m_random.nextInt(); // warm up
    
    for (size_t i = 0; i < m_numTaps; ++i) {
        // calculate tap position
        float r; // random variable
        if (i == 0) { // enforce first tap at pre-delay
            r = uniformStart;
        }
        else {
            // r uniform in [uniformStart, 1]
            r = m_random.nextFloat();
            r = r * (1.f - uniformStart) + uniformStart;
        }
        r = std::cbrtf(r);

        int delaySamples = int(r * tailDelaySamples);
        delaySamples = std::max(1, delaySamples);

        // calculate amplitude with decay
        float gain = std::powf(10, -3.f * r * tailDelayMs / erDecayTimeMs);

        // calculate stereo pan
        float pan = 0.5f + width * (m_random.nextFloat() - 0.5f);
        pan = std::clamp(pan, 0.0f, 1.0f);
        if (m_maxNumChannels == 1) {
            pan = 0.5f;
        }

        // store ER tap
        m_tapDelays[0][i] = delaySamples;
        m_tapGains[0][i] = gain * std::sqrt(1.0f - pan);

        if (m_maxNumChannels == 2) {
            m_tapDelays[1][i] = delaySamples;
            m_tapGains[1][static_cast<size_t>(i)] = gain * std::sqrt(pan);
        }
    }
}

void EarlyReflections::processBlock(const juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& wetBuffer)
{
    int numChannels = std::min(dryBuffer.getNumChannels(), m_maxNumChannels);
    int numSamples = dryBuffer.getNumSamples();
    
    for (int ch = 0; ch < numChannels; ++ch) {
        const float* dryData = dryBuffer.getReadPointer(ch);
        float* wetData = wetBuffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i) {
            // push dry sample into delay line
            m_delayLine.pushSample(ch, dryData[i]);
            
            if (m_numTaps > 0) {
                // sum all taps
                for (int tap = 0; tap < m_numTaps; ++tap) {
                    bool isLastTap = (tap == m_numTaps - 1);
                    float delayed = m_delayLine.popSample(ch, static_cast<float>(m_tapDelays[static_cast<size_t>(ch)][static_cast<size_t>(tap)]), isLastTap);
                    wetData[i] += delayed * m_tapGains[static_cast<size_t>(ch)][static_cast<size_t>(tap)];
                }
            } else {
                // no taps, still advance read pointer
                m_delayLine.popSample(ch, 0.0f, true);
            }
        }
    }
}
