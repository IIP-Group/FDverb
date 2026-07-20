// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include "Fdverb.h"

class FDverbAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener, public juce::AsyncUpdater
{
public:
    FDverbAudioProcessor();
    ~FDverbAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void updateEnvelopeDisplay();
    static constexpr int kNumDisplayBins = 96;
    std::array<std::atomic<float>, kNumDisplayBins> envelopeDisplay;

    juce::AudioProcessorValueTreeState apvts;

    // debug function
    int getCurrentFftSize() const;

    float getMinTailDelayMs() const;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

private:
    Fdverb m_fdverb;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<int> m_pendingFftSize { -1 };  // -1 means no pending change
    static int fftSizeFromIndex(int index);

    // Preset management
    struct PresetDescriptor {
        const char* name;
        const char* xmlData;
        int         xmlSize;
    };
    static const PresetDescriptor kPresets[];
    static const int kNumPresets;
    int m_currentProgram { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FDverbAudioProcessor)
};
