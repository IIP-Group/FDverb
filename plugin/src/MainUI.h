// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>
#include <BinaryData.h>
#include "PluginProcessor.h"
#include "FdverbLookAndFeel.h"
#include "Utils.h"
#include "SpectralDisplay.h"

namespace HelperFunctions
{
    inline void configureSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        slider.setTextBoxIsEditable(true);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    inline void configureGainSlider(juce::Slider& slider)
    {
        slider.textFromValueFunction = [](double value)
        {
            if (value <= FdverbConstants::SILENCE_THRESHOLD_DB)
                return juce::String("mute");
            return juce::String(value, 1) + " dB";
        };

        slider.valueFromTextFunction = [](const juce::String& text)
        {
            if (text.equalsIgnoreCase("mute"))
                return -100.0;
            return text.retainCharacters("-0123456789.").getDoubleValue();
        };
    }

    inline void configureDelaySlider(juce::Slider& slider)
    {
        slider.textFromValueFunction = [](double value)
        {
            return juce::String(static_cast<int>(std::round(value))) + " ms";
        };

        slider.valueFromTextFunction = [](const juce::String& text)
        {
            return text.retainCharacters("0123456789.").getDoubleValue();
        };
    }

    inline void configureFrequencySlider(juce::Slider& slider)
    {
        slider.textFromValueFunction = [](double value)
        {
            if (value >= 1000.0)
                return juce::String(value / 1000.0, 1) + " kHz";
            return juce::String(static_cast<int>(std::round(value))) + " Hz";
        };

        slider.valueFromTextFunction = [](const juce::String& text)
        {
            if (text.containsIgnoreCase("k"))
                return text.retainCharacters("0123456789.").getDoubleValue() * 1000.0;
            return text.retainCharacters("0123456789.").getDoubleValue();
        };
    }
}


// ===============================================================================================


class TitleWithLinesComponent : public juce::Component
{
public:
    TitleWithLinesComponent(const juce::String& titleText = "Title");
    ~TitleWithLinesComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setTitle(const juce::String& newTitle);

private:
    juce::Label titleLabel;
    TitleLookAndFeel titleLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitleWithLinesComponent)
};


// ===============================================================================================


class FDverbTitleComponent : public juce::Component
{
public:
    FDverbTitleComponent();
    ~FDverbTitleComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    juce::Image logoImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FDverbTitleComponent)
};


// ===============================================================================================


class EthLogoComponent : public juce::Component
{
public:
    EthLogoComponent()
    {
        logoImage = juce::ImageCache::getFromMemory(BinaryData::eth_logo_png, BinaryData::eth_logo_pngSize);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    ~EthLogoComponent() override = default;

    void paint(juce::Graphics& g) override
    {
        if (m_showCredits)
        {
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
            g.setColour(MyColours::accentColour.withAlpha(0.8f));
            g.drawFittedText(
                "developed by Nishanth Kumar, Silvan Krebs, David Wieland, "
                "Jonas Roth, and Christoph Studer at the Integrated Information "
                "Processing (IIP) group",
                getLocalBounds(), juce::Justification::centred, 6);
        }
        else
        {
            if (logoImage.isValid())
                g.drawImageWithin(logoImage, 25, 25, getWidth() - 50, getHeight() - 50,
                                  juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        m_showCredits = !m_showCredits;
        repaint();
    }

    void resized() override {}

private:
    juce::Image logoImage;
    bool m_showCredits = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EthLogoComponent)
};


// ===============================================================================================


class FreezeTitleComponent : public juce::Component
{
public:
    FreezeTitleComponent(juce::AudioProcessorValueTreeState& apvts);
    ~FreezeTitleComponent() override = default;

    void resized() override;

private:
    juce::AudioProcessorValueTreeState& m_apvts;

    juce::TextButton freezeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    juce::Label gateLabel;
    juce::ToggleButton gateButton;
    juce::Label gateValueLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreezeTitleComponent)
};


// ===============================================================================================


class FFTSizeComponent : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    FFTSizeComponent(juce::AudioProcessorValueTreeState& apvts, FDverbAudioProcessor& p);
    ~FFTSizeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& m_apvts;
    FDverbAudioProcessor& audioProcessor;

    std::unique_ptr<TitleWithLinesComponent> titleComponent;
    juce::Slider fftsizeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fftsizeAttachment;
    juce::Label msLabel;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateMsLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFTSizeComponent)
};


// ===============================================================================================


class EnvelopeComponent : public juce::Component
{
public:
    EnvelopeComponent(juce::AudioProcessorValueTreeState& apvts);
    ~EnvelopeComponent() override = default;

    void resized() override;

private:
    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::Label modeLabel;
    juce::ToggleButton modeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modeAttachment;
    juce::Label modeValueLabel;

    juce::Slider colorSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> colorAttachment;
    juce::Label colorLabel;

    void updateModeLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeComponent)
};


// ===============================================================================================


class DecayTiltComponent : public juce::Component
{
public:
    DecayTiltComponent(juce::AudioProcessorValueTreeState& apvts);
    ~DecayTiltComponent() override;

    void resized() override;

private:
    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::Slider decaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    juce::Label decayLabel;

    juce::Slider tiltSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tiltAttachment;
    juce::Label tiltLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecayTiltComponent)
};


// ===============================================================================================


class ShiftDriftComponent : public juce::Component,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    ShiftDriftComponent(juce::AudioProcessorValueTreeState& apvts, FDverbAudioProcessor& p);
    ~ShiftDriftComponent() override;

    void resized() override;

private:
    juce::AudioProcessorValueTreeState& m_apvts;
    FDverbAudioProcessor& audioProcessor;

    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::Slider shiftSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shiftAttachment;
    juce::Label shiftLabel;
    juce::Label shiftHzLabel;

    juce::Slider driftSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driftAttachment;
    juce::Label driftLabel;
    juce::Label driftHzSLabel;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void clampShiftDrift();
    void updateSecondaryLabels();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShiftDriftComponent)
};


// ===============================================================================================


class LinkWidthComponent : public juce::Component
{
public:
    LinkWidthComponent(juce::AudioProcessorValueTreeState& apvts);
    ~LinkWidthComponent() override;

    void resized() override;

private:
    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::ToggleButton linkButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;
    juce::Label linkLabel;

    juce::Slider widthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;
    juce::Label widthLabel;
    juce::Label widthValueLabel;

    juce::Label linkValueLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinkWidthComponent)
};


// ===============================================================================================


class BrickEqComponent : public juce::Component
{
public:
    BrickEqComponent(juce::AudioProcessorValueTreeState& apvts);
    ~BrickEqComponent() override;

    void resized() override;

private:
    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::Slider lowcutSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowcutAttachment;
    juce::Label lowcutLabel;

    juce::Slider highcutSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highcutAttachment;
    juce::Label highcutLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrickEqComponent)
};


// ===============================================================================================


class ERComponent : public juce::Component
{
public:
    ERComponent(juce::AudioProcessorValueTreeState& apvts);
    ~ERComponent() override;

    void resized() override;

private:
    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::Slider erTapsSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> erTapsAttachment;
    juce::Label erTapsLabel;

    juce::Slider predelaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> predelayAttachment;
    juce::Label predelayLabel;

    juce::Slider tailDelaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tailDelayAttachment;
    juce::Label tailDelayLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ERComponent)
};


// ===============================================================================================


class ErPreTailComponent : public juce::Component
{
public:
    ErPreTailComponent(juce::AudioProcessorValueTreeState& apvts, FDverbAudioProcessor& p);
    ~ErPreTailComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::Slider erTapsSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> erTapsAttachment;
    juce::Label erTapsLabel;

    juce::Slider predelaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> predelayAttachment;
    juce::Label predelayLabel;
    juce::Rectangle<int> m_predelaySubtitleBounds;

    juce::Slider tailDelaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tailDelayAttachment;
    juce::Label tailDelayLabel;

    FDverbAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErPreTailComponent)
};


// ===============================================================================================


class MixComponent : public juce::Component
{
public:
    MixComponent(juce::AudioProcessorValueTreeState& apvts);
    ~MixComponent() override;

    void resized() override;

private:
    std::unique_ptr<TitleWithLinesComponent> titleComponent;

    juce::Slider drySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryAttachment;
    juce::Label dryLabel;

    juce::Slider erLevelSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> erLevelAttachment;
    juce::Label erLevelLabel;

    juce::Slider tailSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tailAttachment;
    juce::Label tailLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixComponent)
};


// ===============================================================================================


class MainUI : public juce::Component, private juce::Timer
{
public:
    MainUI(FDverbAudioProcessor& p);
    ~MainUI() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    FDverbAudioProcessor& audioProcessor;
    FdverbLookAndFeel fdverbLookAndFeel;

    void timerCallback() override;

    static constexpr bool kShowDevInfo = false;

    // Title bar
    std::unique_ptr<FDverbTitleComponent> titleComponent;
    std::unique_ptr<FreezeTitleComponent> freezeTitleComponent;
    std::unique_ptr<EthLogoComponent> ethLogoComponent;
    juce::Label versionLabel;
    juce::Label buildIdLabel;
    juce::Label effectiveFftSizeLabel;

    // Visualization
    std::unique_ptr<SpectralDisplay> spectralDisplay;

    // Left column
    std::unique_ptr<FFTSizeComponent> fftsizeComponent;
    std::unique_ptr<DecayTiltComponent> decayTiltComponent;
    std::unique_ptr<LinkWidthComponent> linkWidthComponent;

    // Middle column
    std::unique_ptr<EnvelopeComponent> envelopeComponent;
    std::unique_ptr<ShiftDriftComponent> shiftDriftComponent;
    std::unique_ptr<BrickEqComponent> brickEqComponent;

    // Right column
    std::unique_ptr<ErPreTailComponent> erPreTailComponent;
    std::unique_ptr<MixComponent> mixComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainUI)
};
