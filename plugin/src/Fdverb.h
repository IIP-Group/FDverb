// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#ifdef FDVERB_MEASURE_TIMING
#include <chrono>
#include <fstream>
#endif

#include "LockFreeFifo.h"
#include "EnvelopeFollower.h"
#include "EarlyReflections.h"

class Fdverb : private juce::Thread
{
public:
    Fdverb();
    ~Fdverb() override;

    void prepare(double sampleRate, int samplesPerBlock, int maxNumInputChannels);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void requestReset();  // drop all reverb state, callable from any thread
    bool isResetPending() const { return m_resetRequested.load(); }

    void setDryLevelDb(float db);
    void setTailLevelDb(float db);
    void setPredelayMs(float ms);
    void setDecayMs(float ms);
    void setTilt(float tilt);
    void setLowCut(float hz);
    void setHighCut(float hz);
    void setLinkEnvelopes(bool link);
    void setTaildelayMs(float ms);
    void setWidth(float width);
    void setNumERTaps(int taps);
    void setERLevelDb(float db);
    void setColor(float color);
    void setShift(int shift);
    void setDrift(float drift);
    void setEnvelopeMode(std::string& mode);
    void setFreezeInput(bool freeze);
    void setFreeze(bool freeze);

    const float* getEnvelope(int channel) const;
    int getEnvelopeSize() const;

    // debug function
    int getCurrentFftSize() const;

    void setFFTSizeAndReprepare(int newN);

    float getMinTailDelayMs() const;

private:
    void run() override;

    void processWorkBuffer();
    void processFrame();
    void setupMLTWindow();
    void resetWorkerState();  // worker thread only

private:
    double m_sampleRate = 44100.0;
    int m_samplesPerBlock = 512;
    int m_maxNumInputChannels = 2;
    std::atomic<int> m_numInputChannels = 0; // updated at runtime
    std::atomic<int> m_observedNumSamples { 0 }; // host block length, written on the audio thread

    // buffers
    std::vector<std::unique_ptr<LockFreeFifo>> m_sharedInputBuffer;
    std::vector<std::unique_ptr<LockFreeFifo>> m_sharedOutputBuffer;

    juce::AudioBuffer<float> m_dryBuffer;
    juce::AudioBuffer<float> m_workingBuffer;
    juce::AudioBuffer<float> m_overlapAddBuffer;
    std::vector<float> m_fftBuffer;
    juce::AudioBuffer<float> m_erBuffer;
    juce::AudioBuffer<float> m_inputHistory;  // blockSize per channel, worker-local
    std::vector<std::vector<float>> m_noiseBuffer;

    // other
    std::vector<float> m_mltWindow;
    std::unique_ptr<juce::dsp::FFT> m_fft;
    EnvelopeFollower m_envelopeFollower;
    juce::Random m_random;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> m_tailDelayLine;
    EarlyReflections m_earlyReflections;

    juce::WaitableEvent m_signal;
    std::atomic<bool> m_exit { false };

    // Reset handshake: requestReset() raises m_resetRequested, the worker clears its own state and
    // confirms with m_workerResetDone, the audio thread clears its own state and drops the request.
    std::atomic<bool> m_resetRequested { false };
    std::atomic<bool> m_workerResetDone { false };

    // parameters
    int m_blockSize = 8192;
    int m_hopSize = m_blockSize/2;
    float m_decayMs = 1000.f;
    float m_lowCutHz = 20.0f;
    float m_highCutHz = 20000.0f;
    float m_predelayMs = 0.0f;
    float m_dryLevelDb = -6.0f;
    float m_tailLevelDb = -12.0f;
    float m_taildelayMs = 0.0f;
    float m_width = 0.5f;
    int m_numERTaps = 0;
    float m_erLevelDb = -12.0f;
    bool m_freezeInput = false;

    std::atomic<bool> m_isPrepared { false };

#ifdef FDVERB_LOG_BLOCKSIZE
    void logBlockSize();         // worker thread only
    int m_loggedNumSamples = 0;  // worker thread only
#endif

#ifdef FDVERB_MEASURE_TIMING
    std::ofstream m_timingFile;
    int m_timingBlockSize = -1;
#endif
};
