// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <vector>
#include <string>

class EnvelopeFollower
{
public:
    EnvelopeFollower();
    ~EnvelopeFollower();

    void prepare(int numBins, int maxNumChannels, double sampleRate, int blockSize, int hopSize);
    void reset();

    void process(int channel, const float* fftData, int fftSize);

    const float* getEnvelope(int channel) const;
    float getEnvelopeAt(int channel, int bin) const;

    void linkEnvelopes(); // operates on first 2 envelopes, caller is responsible to only call this function if num channels == 2

    void setDecay(float ms);
    void setEnvelopeMode(const std::string& mode);
    void setTilt(float tilt);
    void setLinkEnvelopes(bool link);
    void setShift(int shift);
    void setDrift(int drift);
    void setColor(float color);
    void setFreeze(bool freeze);
    void setFreezeGateClosed(bool closed);

private:
    void updateCoefficients();

    std::vector<std::vector<float>> m_envelopes;
    int m_maxNumChannels = 0;

    float m_sampleRate = 44100.0;
    int m_blockSize = 8192;
    int m_hopSize = m_blockSize/2;
    int m_numBins = 0;

    float m_attackMs = 10.0f;
    float m_decayMs = 500.0f; // rt60
    float m_attackCoeff = 0.0f;
    std::vector<float> m_decayCoeffs;
    float m_tilt = 1.0f;
    bool m_linkEnvelopes = false;
    int m_shift = 0;
    int m_drift = 0;
    float m_color = 0.0f;
    bool m_freeze = false;
    bool m_freezeGateClosed = false;

    std::string m_envelopeMode = "exponential";
};
