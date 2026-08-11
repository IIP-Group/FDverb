// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include <array>
#include "PluginProcessor.h"
#include "EarlyReflections.h"
#include "SpectralDisplay.h"

// Plots the early-reflection impulse response over the envelope-gram while the user is adjusting
// one of the knobs that rebuild the tap table, then fades back out. Overlays the spectral display
// rather than replacing it, so the spectrogram keeps scrolling underneath and shows no gap.
// The envelope-gram's ErPlot mode holds the same plot on screen for as long as the mode lasts.
class ErDisplay : public juce::Component, private juce::Timer
{
public:
    ErDisplay(FDverbAudioProcessor& processor, const SpectralDisplay& envelopeGram);
    ~ErDisplay() override;

    void paint(juce::Graphics& g) override;

    // Holds the plot on screen indefinitely, for the envelope-gram's ErPlot mode. The spectrogram
    // idles in that mode, so the plot drops its translucent backdrop and takes the canvas outright.
    void setConstant(bool shouldBeConstant);

    // Recomputes the taps, fades the plot in, and restarts the hold before it fades back out. Does
    // not summon the plot when the early reflections are switched off (echoes = 0).
    void showTemporarily();

    // Same, but only for a plot that is already on screen — for the knobs that reshape the taps
    // without being ER controls themselves.
    void refreshIfVisible();

private:
    void timerCallback() override;
    void updateFromParameters();

    // Level of the reverb tail at the given time, in dB, or below kMinDb before it starts and once
    // it has died. Exponential mode loses 60 dB per decay time, so this is a straight line on the
    // dB axis; arithmetic mode loses a fixed amplitude per millisecond and hits true zero at half
    // the decay time, so it curves and ends hard.
    float tailLevelDbAt(float ms) const;

    static constexpr float kMaxTimeMs = 500.0f;  // matches the pre-delay / tail-delay parameter range
    static constexpr float kMinDb = -60.0f;
    static constexpr int   kTimerHz = 30;
    static constexpr float kHoldMs = 5000.0f;
    static constexpr float kFadeMs = 400.0f;
    static constexpr float kBackgroundAlpha = 0.75f;  // lets the envelope-gram show through, dimmed

    FDverbAudioProcessor& m_processor;
    const SpectralDisplay& m_envelopeGram;

    std::array<EarlyReflections::Tap, EarlyReflections::kMaxNumTaps> m_taps {};
    int m_numTaps = 0;
    double m_tapSampleRate = 44100.0;
    float m_preDelayMs = 0.0f;
    float m_tailDelayMs = 0.0f;

    // tail region: starts at the tail delay or the FFT latency floor, whichever is later
    float m_tailStartMs = 0.0f;
    float m_tailLevelDb = 0.0f;
    float m_decayMs = 1000.0f;
    bool m_arithmeticEnvelope = false;
    bool m_frozen = false;  // freeze holds the envelope, so the region stops decaying

    float m_alpha = 0.0f;
    float m_holdMsRemaining = 0.0f;
    bool m_constant = false;  // held on screen by the envelope-gram's ErPlot mode

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErDisplay)
};
