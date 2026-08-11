// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpectralDisplay : public juce::Component, private juce::Timer
{
public:
    // Clicking the canvas cycles through these in order. ErPlot hands the canvas to the ER plot,
    // which keeps drawing itself until the next click; the spectrogram idles as it does when off.
    enum class DisplayState { GrayMap, ColorMap, ErPlot, Off };

    SpectralDisplay(FDverbAudioProcessor& processor);
    ~SpectralDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void mouseUp(const juce::MouseEvent&) override;

    // the ER plot overlays this canvas and follows its colour map, so the two always match
    bool isColourMode() const { return m_displayState == DisplayState::ColorMap; }
    bool isErPlotMode() const { return m_displayState == DisplayState::ErPlot; }

    // Fired after a click has moved the cycle on, so the ER plot can pick up its constant mode
    std::function<void()> onDisplayStateChanged;

private:
    FDverbAudioProcessor& m_processor;

    juce::Image m_spectrogram;
    int m_writePos = 0;
    DisplayState m_displayState = DisplayState::GrayMap;

    // Greeting logo: overlaid on the empty canvas, scrolls along, white/color version
    juce::Image m_logoWhite, m_logoColour;
    juce::Rectangle<float> m_logoArea;
    int m_logoScrollPx = 0;

    void drawLogo(juce::Graphics& g);
    juce::Colour getColourForValue(float normalized);
};
