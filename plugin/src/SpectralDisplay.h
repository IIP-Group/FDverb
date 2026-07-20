// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpectralDisplay : public juce::Component, private juce::Timer
{
public:
    enum class DisplayState { GrayMap, ColorMap, Off };

    SpectralDisplay(FDverbAudioProcessor& processor);
    ~SpectralDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    FDverbAudioProcessor& m_processor;

    juce::Image m_spectrogram;
    int m_writePos = 0;
    DisplayState m_displayState = DisplayState::GrayMap;

    juce::Colour getColourForValue(float normalized);
};
