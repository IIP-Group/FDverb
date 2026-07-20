// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cmath>

#include "Fdverb.h"
#include "Utils.h"

Fdverb::Fdverb(juce::AudioProcessorValueTreeState& apvts)
    : juce::Thread("Fdverb Worker"), m_apvts(apvts)
{
}

Fdverb::~Fdverb()
{
    m_exit.store(true);
    m_signal.signal();
    stopThread(2000);
}

void Fdverb::run()
{
    while (!threadShouldExit() && !m_exit.load()) {
        m_signal.wait(100);

        if (threadShouldExit() || m_exit.load()) {
            break;
        }

        while (m_sharedInputBuffer[0]->getNumReady() >= m_hopSize
               && !threadShouldExit()
               && !m_exit.load()) {
            processFrame();
        }
    }
}

void Fdverb::prepare(double sampleRate, int samplesPerBlock, int maxNumInputChannels)
{
    DBG("prepare called with m_blockSize = " + juce::String(m_blockSize));
    m_isPrepared.store(false);

    if (isThreadRunning()) {
        m_exit.store(true);
        m_signal.signal();
        stopThread(2000);
    }

    m_sampleRate = sampleRate;
    m_samplesPerBlock = samplesPerBlock;
    m_maxNumInputChannels = maxNumInputChannels;

    // init to max number of channels, only access used number of channels (available at runtime)
    m_sharedInputBuffer.clear();
    m_sharedOutputBuffer.clear();

    for (int i = 0; i < maxNumInputChannels; ++i) {
        m_sharedInputBuffer.push_back(std::make_unique<LockFreeFifo>());
    }
    for (auto& ch : m_sharedInputBuffer) {
        ch->resize(8 * m_blockSize + samplesPerBlock);
    }
    for (int i = 0; i < maxNumInputChannels; ++i) {
        m_sharedOutputBuffer.push_back(std::make_unique<LockFreeFifo>());
    }
    for (auto& ch : m_sharedOutputBuffer) {
        ch->resize(8 * m_blockSize + samplesPerBlock);
    }

    m_dryBuffer.setSize(maxNumInputChannels, samplesPerBlock);
    m_dryBuffer.clear();
    m_workingBuffer.setSize(maxNumInputChannels, m_blockSize);
    m_workingBuffer.clear();
    m_overlapAddBuffer.setSize(maxNumInputChannels, m_blockSize);
    m_overlapAddBuffer.clear();
    m_inputHistory.setSize(maxNumInputChannels, m_blockSize);
    m_inputHistory.clear();

    setupMLTWindow();

    int fftOrder = static_cast<int>(std::log2(m_blockSize));
    m_fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    m_fftBuffer.resize(static_cast<size_t>(2 * m_blockSize));

    m_noiseBuffer.assign(static_cast<size_t>(maxNumInputChannels), std::vector<float>(m_fftBuffer.size(), 0.0f));

    m_envelopeFollower.prepare(m_blockSize/2+1, m_maxNumInputChannels, m_sampleRate, m_blockSize, m_hopSize);

    int maxExtraTailDelaySamples = static_cast<int>(1.0f * m_sampleRate); // 1 second max
    m_tailDelayLine.setMaximumDelayInSamples(maxExtraTailDelaySamples);
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels = static_cast<uint32_t>(maxNumInputChannels);
    m_tailDelayLine.prepare(spec);
    m_tailDelayLine.reset();

    m_earlyReflections.prepare(sampleRate, samplesPerBlock, maxNumInputChannels, 32, 1000.0f); // 32 taps max, 1000ms max delay (max predelay + max taildelay)
    m_erBuffer.setSize(maxNumInputChannels, samplesPerBlock);

    m_exit.store(false);
    startThread(juce::Thread::Priority::high);

    m_isPrepared.store(true);
}

void Fdverb::processBlock(juce::AudioBuffer<float>& buffer)
{
    m_numInputChannels = buffer.getNumChannels();

    // copy dry signal for final mix
    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        m_dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }

    // freeze input
    if (m_freezeInput) {
        buffer.clear();
    }

    // early reflections
    m_erBuffer.clear();
    m_earlyReflections.calculateTaps(m_predelayMs, m_taildelayMs, m_decayMs, juce::jmin(m_width, 1.0f), m_numERTaps);
    m_earlyReflections.processBlock(m_dryBuffer, m_erBuffer);

    // write incoming samples to ring buffer
    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        m_sharedInputBuffer[ch]->write(buffer.getReadPointer(ch), buffer.getNumSamples());
    }

    if (m_sharedInputBuffer[0]->getNumReady() >= m_hopSize) {
        m_signal.signal();
    }

    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        int got = m_sharedOutputBuffer[ch]->read(buffer.getWritePointer(ch), buffer.getNumSamples());
        if (got < buffer.getNumSamples()) {
            DBG("underrun: " << (buffer.getNumSamples() - got));
            juce::FloatVectorOperations::clear(
                buffer.getWritePointer(ch) + got,
                buffer.getNumSamples() - got);
        }
    }


    // extra delay for tail signal
    float extraDelayMs = m_taildelayMs - getMinTailDelayMs();
    extraDelayMs = std::max(0.f, extraDelayMs); // double-check for safety (should be enforced by UI)
    int extraDelaySamples = extraDelayMs * 0.001f * static_cast<float>(m_sampleRate);
    extraDelaySamples = std::max(1, extraDelaySamples);
    m_tailDelayLine.setDelay(extraDelaySamples);

    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            float in = data[i];
            data[i] = m_tailDelayLine.popSample(ch);
            m_tailDelayLine.pushSample(ch, in);
        }
    }

    //
    // mixing section
    //

    float tailLevelGain = dbToLinear(m_tailLevelDb);
    float dryLevelGain = dbToLinear(m_dryLevelDb);
    float erLevelGain = dbToLinear(m_erLevelDb);

    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        // buffer currently contains wet signal, add gain
        buffer.applyGain(ch, 0, buffer.getNumSamples(), tailLevelGain);

        // add dry signal with gain
        buffer.addFrom(ch, 0, m_dryBuffer, ch, 0, buffer.getNumSamples(), dryLevelGain);

        // add er signal with gain
        buffer.addFrom(ch, 0, m_erBuffer, ch, 0, buffer.getNumSamples(), erLevelGain);
    }
}

void Fdverb::setDryLevelDb(float db) { m_dryLevelDb = db; }
void Fdverb::setTailLevelDb(float db) { m_tailLevelDb = db; }
void Fdverb::setPredelayMs(float ms) { m_predelayMs = ms; }
void Fdverb::setDecayMs(float ms) { m_decayMs = ms; m_envelopeFollower.setDecay(ms); }
void Fdverb::setTilt(float tilt) { m_envelopeFollower.setTilt(tilt); }
void Fdverb::setLowCut(float hz) { m_lowCutHz = hz; }
void Fdverb::setHighCut(float hz) { m_highCutHz = hz; }
void Fdverb::setLinkEnvelopes(bool link) { m_envelopeFollower.setLinkEnvelopes(link); }
void Fdverb::setTaildelayMs(float ms) { m_taildelayMs = ms; }
void Fdverb::setWidth(float width) { m_width = width; }
void Fdverb::setNumERTaps(int taps) { m_numERTaps = taps; }
void Fdverb::setERLevelDb(float db) { m_erLevelDb = db; }
void Fdverb::setColor(float color) { m_envelopeFollower.setColor(color); }
void Fdverb::setShift(int shift) { m_envelopeFollower.setShift(shift); }
void Fdverb::setDrift(int drift) { m_envelopeFollower.setDrift(drift); }
void Fdverb::setEnvelopeMode(std::string& mode) { m_envelopeFollower.setEnvelopeMode(mode); }
void Fdverb::setFreezeInput(bool freeze) { m_freezeInput = freeze; m_envelopeFollower.setFreezeGateClosed(freeze); }
void Fdverb::setFreeze(bool freeze) { m_envelopeFollower.setFreeze(freeze); }

void Fdverb::processFrame()
{
#ifdef FDVERB_MEASURE_TIMING
    auto t0 = std::chrono::high_resolution_clock::now();
#endif

    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        float* hist = m_inputHistory.getWritePointer(ch);

        // shift input history left by hopSize
        std::memmove(hist, hist + m_hopSize, static_cast<size_t>(m_blockSize - m_hopSize) * sizeof(float));

        // read hopSize new samples from shared input fifo into last hopSize samples of history buffer
        m_sharedInputBuffer[ch]->read(hist + (m_blockSize - m_hopSize), m_hopSize);

        // copy all blockSize samples from history to working buffer
        float* work = m_workingBuffer.getWritePointer(ch);
        std::memcpy(work, hist, static_cast<size_t>(m_blockSize) * sizeof(float));
    }

    // fdverb processing
    processWorkBuffer();

    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        // copy all blockSize samples from working buffer to overlap add buffer
        m_overlapAddBuffer.addFrom(ch, 0, m_workingBuffer, ch, 0, m_blockSize);

        // write first hopSize samples to shared output buffer
        float* oa = m_overlapAddBuffer.getWritePointer(ch);
        m_sharedOutputBuffer[ch]->write(oa, m_hopSize);

        // shift overlapp add buffer left by hopSize to discard first hopSize samples
        std::memmove(oa, oa + m_hopSize, static_cast<size_t>(m_blockSize - m_hopSize) * sizeof(float));

        // zero freed up region
        std::memset(oa + (m_blockSize - m_hopSize), 0, static_cast<size_t>(m_hopSize) * sizeof(float));
    }

#ifdef FDVERB_MEASURE_TIMING
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (m_timingBlockSize != m_blockSize) {
        if (m_timingFile.is_open())
            m_timingFile.close();
        auto dir = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                       .getChildFile("fdverb_runtime_measurements");
        dir.createDirectory();
        auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        auto path = dir.getChildFile(timestamp + "_blocksize_" + juce::String(m_blockSize) + ".txt");
        m_timingFile.open(path.getFullPathName().toStdString(), std::ios::app);
        m_timingBlockSize = m_blockSize;
    }
    if (m_timingFile.is_open())
        m_timingFile << "processFrame: " << ms << " ms  (N=" << m_blockSize << ")\n";
#endif
}

void Fdverb::processWorkBuffer()
{
    int lowCutBin = static_cast<int>(m_lowCutHz * m_blockSize / m_sampleRate);
    int highCutBin = static_cast<int>(m_highCutHz * m_blockSize / m_sampleRate);

    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        float* data = m_workingBuffer.getWritePointer(ch);

        // apply mlt window
        for (int i = 0; i < m_blockSize; ++i) {
            data[i] *= m_mltWindow[static_cast<size_t>(i)];
        }

        // forward fft
        std::memcpy(m_fftBuffer.data(), m_workingBuffer.getReadPointer(ch), static_cast<size_t>(m_blockSize) * sizeof(float));
        std::memset(m_fftBuffer.data() + m_blockSize, 0, static_cast<size_t>(m_blockSize) * sizeof(float)); // zero-padding

        m_fft->performRealOnlyForwardTransform(m_fftBuffer.data());

        // normalize
        for (size_t i = 0; i < static_cast<size_t>(m_blockSize+2); ++i) {
            m_fftBuffer[i] *= 1.0f / static_cast<float>(std::sqrt(m_blockSize));
        }

        // envelope following
        m_envelopeFollower.process(ch, m_fftBuffer.data(), m_fft->getSize());
    }

    // cross coupling
    if (m_numInputChannels == 2) {
        m_envelopeFollower.linkEnvelopes();
    }

    // generate raw noise spectra (with hard EQ), per channel
    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        float* spec = m_noiseBuffer[static_cast<size_t>(ch)].data();
        for (size_t i = 0; i < static_cast<size_t>(m_blockSize/2+1); ++i) {
            if (static_cast<int>(i) < lowCutBin || static_cast<int>(i) > highCutBin) {
                spec[2*i]     = 0.0f;
                spec[2*i + 1] = 0.0f;
            } else {
                spec[2*i]     = m_random.nextFloat() * 2.0f - 1.0f;
                spec[2*i + 1] = m_random.nextFloat() * 2.0f - 1.0f;
            }
        }
    }

    // M/S stereo width: 0=mid only, 1=mid+side equally, 2=side only
    // midGain: 1→1→0, sideGain: 0→1→1 as width goes 0→1→2
    // since noise is uncorrelated, we can directly interpret the L/R noise as M/S
    if (m_numInputChannels == 2) {
        const float midGain = (m_width <= 1.0f) ? 1.0f : (2.0f - m_width);
        const float sideGain = (m_width <= 1.0f) ? m_width : 1.0f;
        float* L = m_noiseBuffer[0].data();
        float* R = m_noiseBuffer[1].data();
        for (size_t k = 0; k < static_cast<size_t>(m_blockSize+2); ++k) {
            const float mid  = L[k];
            const float side = R[k];
            L[k] = midGain * mid  + sideGain * side;
            R[k] = midGain * mid  - sideGain * side;
        }
    }

    // shape by envelope, inverse FFT, normalize, output, window
    for (int ch = 0; ch < m_numInputChannels; ++ch) {
        const float* env  = m_envelopeFollower.getEnvelope(ch);
        const float* spec = m_noiseBuffer[ch].data();

        for (size_t i = 0; i < static_cast<size_t>(m_blockSize/2+1); ++i) {
            m_fftBuffer[2*i]     = spec[2*i]     * env[i];
            m_fftBuffer[2*i + 1] = spec[2*i + 1] * env[i];
        }

        m_fft->performRealOnlyInverseTransform(m_fftBuffer.data());

        for (size_t i = 0; i < static_cast<size_t>(m_blockSize+2); ++i) {
            m_fftBuffer[i] *= static_cast<float>(std::sqrt(m_blockSize));
        }

        std::memcpy(m_workingBuffer.getWritePointer(ch), m_fftBuffer.data(),
                    static_cast<size_t>(m_blockSize) * sizeof(float));

        float* data = m_workingBuffer.getWritePointer(ch);
        for (int i = 0; i < m_blockSize; ++i)
            data[i] *= m_mltWindow[static_cast<size_t>(i)];
    }
}

void Fdverb::setupMLTWindow()
{
    m_mltWindow.assign(static_cast<size_t>(m_blockSize), 0.0f);

    for (size_t i = 0; i < static_cast<size_t>(m_blockSize); ++i) {
        m_mltWindow[i] = static_cast<float>(std::sin(juce::MathConstants<float>::pi * (i + 0.5f) / m_blockSize));
    }
}

const float* Fdverb::getEnvelope(int channel) const
{
    return m_envelopeFollower.getEnvelope(channel);
}

int Fdverb::getEnvelopeSize() const
{
    return m_blockSize / 2 + 1;
}

int Fdverb::getCurrentFftSize() const
{
    return m_workingBuffer.getNumSamples();
}

void Fdverb::setFFTSizeAndReprepare(int newN)
{
    if (newN == m_blockSize) {
        return;
    }

    if (!m_isPrepared.load()) {
        m_blockSize = newN;
        m_hopSize = newN / 2;
        return;
    }

    // Stop the worker BEFORE touching m_blockSize, so it can't observe a mismatched state
    m_exit.store(true);
    m_signal.signal();
    stopThread(2000);

    m_blockSize = newN;
    m_hopSize = newN / 2;

    prepare(m_sampleRate, m_samplesPerBlock, m_maxNumInputChannels);

    float newFloor = getMinTailDelayMs();
    if (auto* tailParam = m_apvts.getParameter("TAILDELAY")) {
        float current = tailParam->convertFrom0to1(tailParam->getValue());
        if (current < newFloor) {
            tailParam->setValueNotifyingHost(tailParam->convertTo0to1(newFloor));
        }
    }
}

float Fdverb::getMinTailDelayMs() const
{
    if (m_sampleRate <= 0.0) { // catch div-by-zero
        return 0.0f;
    }
    float fixedDelayMs = (static_cast<float>(m_blockSize) / 2.0f + static_cast<float>(m_samplesPerBlock)) / static_cast<float>(m_sampleRate) * 1000.0f;

    // empirical mean processing delay per FFT size
    float processingDelayMs = 0.0f;
    if (m_blockSize <= 32)        processingDelayMs = 0.016f;
    else if (m_blockSize <= 64)   processingDelayMs = 0.036f;
    else if (m_blockSize <= 128)  processingDelayMs = 0.067f;
    else if (m_blockSize <= 256)  processingDelayMs = 0.119f;
    else if (m_blockSize <= 512)  processingDelayMs = 0.464f;
    else if (m_blockSize <= 1024) processingDelayMs = 0.439f;
    else if (m_blockSize <= 2048) processingDelayMs = 0.825f;
    else if (m_blockSize <= 4096) processingDelayMs = 1.500f;
    else if (m_blockSize <= 8192) processingDelayMs = 2.648f;
    else                          processingDelayMs = 4.524f;

    return fixedDelayMs + processingDelayMs;
}
