// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "MainUI.h"

class FDverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FDverbAudioProcessorEditor(FDverbAudioProcessor&);
    ~FDverbAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    FDverbAudioProcessor& audioProcessor;

    MainUI mainUI;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FDverbAudioProcessorEditor)
};
