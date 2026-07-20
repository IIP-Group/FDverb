// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cmath>
#include <algorithm>

#include "EnvelopeFollower.h"

EnvelopeFollower::EnvelopeFollower()
{
}

EnvelopeFollower::~EnvelopeFollower()
{
}

void EnvelopeFollower::prepare(int numBins, int maxNumChannels, double sampleRate, int blockSize, int hopSize)
{
    m_numBins = numBins;
    m_maxNumChannels = maxNumChannels;
    m_sampleRate = static_cast<float>(sampleRate);
    m_blockSize = blockSize;
    m_hopSize = hopSize;

    m_decayCoeffs.assign(static_cast<size_t>(numBins), 1.0f);
    m_envelopes.assign(static_cast<size_t>(maxNumChannels), std::vector<float>(static_cast<size_t>(numBins)));

    updateCoefficients();
}

void EnvelopeFollower::reset()
{
    for (auto& env : m_envelopes) {
        std::fill(env.begin(), env.end(), 0.0f);
    }
}

void EnvelopeFollower::process(int channel, const float* fftData, int fftSize)
{
    // juce fft data is in interleaved format ( [real0, imag0, real1, imag1, ...]

    auto& env = m_envelopes[static_cast<size_t>(channel)];

    // create copy to prevent overwriting for positive drift values
    std::vector<float> env_copy(m_envelopes[static_cast<size_t>(channel)].begin(),
                                 m_envelopes[static_cast<size_t>(channel)].end());

    for (size_t i = 0; i < static_cast<size_t>(m_numBins); ++i) {

        // shift offsets which fft bin you read from
        int srcBin = static_cast<int>(i) - m_shift; // minus for right shift direction
        float mag = 0.f;    // assign default value, used if outside bounds
        if (srcBin >= 0 && srcBin < m_numBins) {
            float re = fftData[2 * srcBin];
            float im = fftData[2 * srcBin + 1];
            mag = std::sqrt(re*re + im*im);
            mag = std::pow(mag, m_color);
        }

        // drift offsets which envelope state you read from
        int envBin = static_cast<int>(i) - m_drift; // same here
        float prevEnv = 0.f;    // assign default value, used if outside bounds
        if (envBin >= 0 && envBin < m_numBins) {
            prevEnv = env_copy[static_cast<size_t>(envBin)];
        }

        if (!m_freeze) {
            if (m_envelopeMode == "exponential") {
                if (mag >= prevEnv) {
                    env[i] = m_attackCoeff * mag + (1.0f - m_attackCoeff) * prevEnv;
                } else {
                    env[i] = m_decayCoeffs[i] * mag + (1.0f - m_decayCoeffs[i]) * prevEnv;
                }
            } else { // arithmetic
                if (mag >= prevEnv) {
                    env[i] = std::min(prevEnv + m_attackCoeff, mag);
                } else {
                    env[i] = std::max(0.0f, std::max(prevEnv - m_decayCoeffs[i], mag));
                }
            }
        } else { // freeze, ignore m_envelopeMode
            // Always apply drift shift
            env[i] = prevEnv;
            // Gate open: louder incoming audio can paint over the drifted value
            if (!m_freezeGateClosed && mag > env[i])
                env[i] = mag;
        }
    }
}

const float* EnvelopeFollower::getEnvelope(int channel) const
{
    return m_envelopes[static_cast<size_t>(channel)].data();
}

float EnvelopeFollower::getEnvelopeAt(int channel, int bin) const
{
    return m_envelopes[static_cast<size_t>(channel)][static_cast<size_t>(bin)];
}

void EnvelopeFollower::linkEnvelopes()
{
    if (!m_linkEnvelopes || m_maxNumChannels < 2) {
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(m_numBins); ++i) {
        float envL = m_envelopes[0][i];
        float envR = m_envelopes[1][i];

        float mixed = 0.5f * envL + 0.5f * envR;

        m_envelopes[0][i] = mixed;
        m_envelopes[1][i] = mixed;
    }
}

void EnvelopeFollower::setDecay(float ms)
{
    m_decayMs = ms;
    updateCoefficients();
}

void EnvelopeFollower::setEnvelopeMode(const std::string& mode)
{
    m_envelopeMode = mode;
}

void EnvelopeFollower::setTilt(float tilt)
{
    m_tilt = tilt;
}

void EnvelopeFollower::setLinkEnvelopes(bool link)
{
    m_linkEnvelopes = link;
}

void EnvelopeFollower::setShift(int shift)
{
    m_shift = shift;
}

void EnvelopeFollower::setDrift(int shift)
{
    m_drift = shift;
}

void EnvelopeFollower::setColor(float color)
{
    m_color = color;
}

void EnvelopeFollower::setFreeze(bool freeze)
{
    m_freeze = freeze;
}

void EnvelopeFollower::setFreezeGateClosed(bool closed)
{
    m_freezeGateClosed = closed;
}

// Convert RT60 to a per-block exponential decay coefficient.
// Args:
//     t60_ms: RT60 reverberation time in milliseconds.
//     fs: Sample rate in Hz.
//     hop_size: Hop size in samples.
// Returns:
//     Per-block decay coefficient; 1.0 if t60_ms <= 0.
inline float tSixtyToCoef(float tSixtyMs, size_t hopSize, float fs)
{
    float blocksPerT60 = tSixtyMs * 0.001f * fs / static_cast<float>(hopSize);
    return blocksPerT60 > 0.0f ? 1.0f - std::pow(10.0f, -3.0f / blocksPerT60) : 1.0f;
}

// Convert decay time to a per-block arithmetic (linear) decay coefficient.
// Args:
//     t1_ms: Time in milliseconds for an increase/decrease by 1.
//     fs: Sample rate in Hz.
//     hop_size: Hop size in samples.
//     slope_scale: tweak slope to match percieved decay time (empirical parameter).
// Returns:
//     Per-block amplitude decrement for linear decay over t1_ms milliseconds.
inline float tOneToCoef(float tOneMs, size_t hopSize, float fs, float slopeScale = 2.0f)
{
    return slopeScale * static_cast<float>(hopSize) / (fs * tOneMs * 0.001f);
}


void EnvelopeFollower::updateCoefficients()
{
    // tilt
    const float k_tiltUpperFreqHz = 20000.f;
    const float k_tiltLowerFreqHz = 20.f;
    const float k_tiltMidFreqHz = std::sqrt(k_tiltUpperFreqHz * k_tiltLowerFreqHz);
    const float k_scaleFactor = 2.0f / std::log10(k_tiltUpperFreqHz / k_tiltLowerFreqHz);

    for (size_t i = 0; i < m_numBins; ++i) {
        float binTilt = 1.f + m_tilt;
        if (i != 0) {
            float binFreqHz = i * static_cast<float>(m_sampleRate) / m_blockSize;
            binTilt = 1.f - k_scaleFactor * std::log10(binFreqHz/k_tiltMidFreqHz) * m_tilt;
        }
        binTilt = std::clamp(binTilt, 0.0f, 2.0f);

        if (m_envelopeMode == "exponential") {
            m_decayCoeffs[i] = tSixtyToCoef(m_decayMs*binTilt, m_hopSize, m_sampleRate);
        } else if (m_envelopeMode == "arithmetic") {
            m_decayCoeffs[i] = tOneToCoef(m_decayMs*binTilt, m_hopSize, m_sampleRate);
        } else { // freeze
            m_decayCoeffs[i] = 1.0f;
        }
    }

    // attack coeff
    m_attackCoeff = tSixtyToCoef(m_attackMs, m_hopSize, m_sampleRate);
}
