// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

FDverbAudioProcessorEditor::FDverbAudioProcessorEditor(FDverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), mainUI(p)
{
    addAndMakeVisible(mainUI);

    setSize(800, 600);
}

FDverbAudioProcessorEditor::~FDverbAudioProcessorEditor()
{
}

void FDverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void FDverbAudioProcessorEditor::resized()
{
    mainUI.setBounds(getLocalBounds());
}
