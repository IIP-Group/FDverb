// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "MainUI.h"
#include "FdverbLookAndFeel.h"

TitleWithLinesComponent::TitleWithLinesComponent(const juce::String& titleText)
{
    titleLabel.setLookAndFeel(&titleLookAndFeel);
    titleLabel.setText(titleText, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
}

TitleWithLinesComponent::~TitleWithLinesComponent()
{
    titleLabel.setLookAndFeel(nullptr);
}

void TitleWithLinesComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (titleLabel.getText().isEmpty()) {
        auto lineArea = bounds.reduced(bounds.getWidth() * 0.10f, 0);
        auto centerY = bounds.getCentreY();
        g.setColour(MyColours::accentColour);
        g.drawLine(lineArea.getX(), centerY, lineArea.getRight(), centerY, 2.0f);
        return;
    }

    auto text = titleLabel.getText();
    auto font = juce::Font(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold);
    g.setFont(font);
    auto textWidth = font.getStringWidth(text);

    auto textPadding = bounds.getWidth() * 0.05f;
    auto lineGap = bounds.getWidth() * 0.02f;
    auto lineMargin = bounds.getWidth() * 0.05f;

    auto textArea = bounds.withSizeKeepingCentre(textWidth + textPadding, bounds.getHeight());
    auto lineArea = bounds.reduced(lineMargin, 0);
    auto leftLineEnd = textArea.getX() - lineGap;
    auto rightLineStart = textArea.getRight() + lineGap;
    auto centerY = bounds.getCentreY();

    g.setColour(MyColours::accentColour);

    if (leftLineEnd > lineArea.getX())
        g.drawLine(lineArea.getX(), centerY, leftLineEnd, centerY, 2.0f);

    if (rightLineStart < lineArea.getRight())
        g.drawLine(rightLineStart, centerY, lineArea.getRight(), centerY, 2.0f);
}

void TitleWithLinesComponent::resized()
{
    titleLabel.setBounds(getLocalBounds());
}

void TitleWithLinesComponent::setTitle(const juce::String& newTitle)
{
    titleLabel.setText(newTitle, juce::dontSendNotification);
    repaint();
}


// ===============================================================================================


FDverbTitleComponent::FDverbTitleComponent()
{
    logoImage = juce::ImageCache::getFromMemory(BinaryData::FDverb_Logo_White_png,
                                                BinaryData::FDverb_Logo_White_pngSize);
}

void FDverbTitleComponent::paint(juce::Graphics& g)
{
    if (logoImage.isValid())
        g.drawImageWithin(logoImage, 5, 5, getWidth()-10, getHeight()-10,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
}

// Old text-based logo with streak shadow effect:
// void FDverbTitleComponent::paint(juce::Graphics& g)
// {
//     auto bounds = getLocalBounds().toFloat();
//     juce::Font titleFont(juce::Font::getDefaultSansSerifFontName(),
//                          bounds.getHeight() * 0.75f,
//                          juce::Font::bold | juce::Font::italic);
//     g.setFont(titleFont);
//     // Streak shadow layers — offset rightward so they displace behind the main text
//     g.setColour(MyColours::accentColour.withAlpha(0.3f));
//     g.drawFittedText("FDverb", bounds.translated(6.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1);
//     g.setColour(MyColours::accentColour.withAlpha(0.25f));
//     g.drawFittedText("FDverb", bounds.translated(3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1);
//     g.setColour(MyColours::accentColour);
//     g.drawFittedText("FDverb", bounds.toNearestInt(), juce::Justification::centred, 1);
// }


// ===============================================================================================


FFTSizeComponent::FFTSizeComponent(juce::AudioProcessorValueTreeState& apvts,
                                   FDverbAudioProcessor& p)
    : m_apvts(apvts), audioProcessor(p)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("block size");
    addAndMakeVisible(*titleComponent);

    fftsizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fftsizeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 30);
    fftsizeSlider.setTextBoxIsEditable(true);
    fftsizeSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    fftsizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "FFTSIZE", fftsizeSlider);

    fftsizeSlider.textFromValueFunction = [](double value)
    {
        return juce::String(32 << static_cast<int>(value));
    };
    fftsizeSlider.valueFromTextFunction = [](const juce::String& text) -> double
    {
        const double n = text.retainCharacters("0123456789").getDoubleValue();
        if (n <= 0.0) return 0.0;
        // find index of closest valid size (32, 64, 128, …, 16384)
        double best = 0.0, bestDist = 1e18;
        for (int i = 0; i <= 9; ++i) {
            const double sz = 32.0 * double(1 << i);
            const double d = n < sz ? sz - n : n - sz;
            if (d < bestDist) { bestDist = d; best = double(i); }
        }
        return best;
    };
    fftsizeSlider.updateText();
    fftsizeSlider.setColour(Slider::textBoxOutlineColourId, MyColours::windowBGColour);
    fftsizeSlider.setColour(Slider::textBoxTextColourId, MyColours::valueTextColour);
    addAndMakeVisible(fftsizeSlider);

    msLabel.setJustificationType(juce::Justification::centred);
    msLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    msLabel.setColour(juce::Label::textColourId, MyColours::valueTextColour.withAlpha(0.55f));
    addAndMakeVisible(msLabel);

    apvts.addParameterListener("FFTSIZE", this);
    updateMsLabel();
}

FFTSizeComponent::~FFTSizeComponent()
{
    m_apvts.removeParameterListener("FFTSIZE", this);
}

void FFTSizeComponent::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync(
        [safeThis = juce::Component::SafePointer<FFTSizeComponent>(this)]() {
            if (safeThis) safeThis->updateMsLabel();
        });
}

void FFTSizeComponent::updateMsLabel()
{
    const double sr = audioProcessor.getSampleRate();
    if (sr <= 0.0)
    {
        msLabel.setText("", juce::dontSendNotification);
        return;
    }
    const double ms = audioProcessor.getCurrentFftSize() * 1000.0 / sr;
    msLabel.setText(juce::String(ms, 1) + " ms", juce::dontSendNotification);
}

void FFTSizeComponent::paint(juce::Graphics& g)
{
}

void FFTSizeComponent::resized()
{
    auto bounds = getLocalBounds();
    titleComponent->setBounds(bounds.removeFromTop(bounds.getHeight() * 0.12f));
    auto sliderBounds = bounds.withSizeKeepingCentre(bounds.getWidth() - 8, 60);
    fftsizeSlider.setBounds(sliderBounds);
    msLabel.setBounds(sliderBounds.getX(), sliderBounds.getBottom(),
                      sliderBounds.getWidth(), 16);
}


// ===============================================================================================


FreezeTitleComponent::FreezeTitleComponent(juce::AudioProcessorValueTreeState& apvts)
    : m_apvts(apvts)
{
    freezeButton.setButtonText("freeze");
    freezeButton.setClickingTogglesState(true);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "FREEZE", freezeButton);
    freezeButton.setButtonText(freezeButton.getToggleState() ? "un-freeze" : "freeze");
    freezeButton.onClick = [this]() {
        freezeButton.setButtonText(freezeButton.getToggleState() ? "un-freeze" : "freeze");
    };
    addAndMakeVisible(freezeButton);

    gateLabel.setText("gate", juce::dontSendNotification);
    gateLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gateLabel);

    gateButton.setClickingTogglesState(true);
    gateButton.setToggleState(*apvts.getRawParameterValue("FREEZEINPUT") < 0.5f,
                              juce::dontSendNotification);
    gateButton.onClick = [this]() {
        m_apvts.getParameterAsValue("FREEZEINPUT").setValue(!gateButton.getToggleState());
        gateValueLabel.setText(gateButton.getToggleState() ? "open" : "closed",
                               juce::dontSendNotification);
    };
    addAndMakeVisible(gateButton);

    gateValueLabel.setText(gateButton.getToggleState() ? "open" : "closed",
                           juce::dontSendNotification);
    gateValueLabel.setJustificationType(juce::Justification::centred);
    gateValueLabel.setColour(juce::Label::textColourId, MyColours::valueTextColour);
    addAndMakeVisible(gateValueLabel);
}

void FreezeTitleComponent::resized()
{
    auto bounds = getLocalBounds();

    // Left: freeze button
    auto freezeArea = bounds.removeFromLeft(bounds.getWidth() * 0.55f);
    freezeButton.setBounds(freezeArea.reduced(freezeArea.getWidth() * 0.05f,
                                               freezeArea.getHeight() * 0.25f));

    // Right: gate label + toggle + value label grouped and vertically centered
    constexpr int kLabelH  = 16;
    constexpr int kToggleH = 25;
    constexpr int kGap     = 5;
    constexpr int kGroupH  = kLabelH + kGap + kToggleH + kGap + kLabelH;
    auto trimmed = bounds.withTrimmedRight(50);     // move closer to freeze button
    auto group = trimmed.withSizeKeepingCentre(trimmed.getWidth(), kGroupH);
    gateLabel.setBounds(group.removeFromTop(kLabelH));
    group.removeFromTop(kGap);
    gateButton.setBounds(group.removeFromTop(kToggleH).withSizeKeepingCentre(45, kToggleH));
    group.removeFromTop(kGap);
    gateValueLabel.setBounds(group.removeFromTop(kLabelH));
}


// ===============================================================================================


EnvelopeComponent::EnvelopeComponent(juce::AudioProcessorValueTreeState& apvts)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("envelope");
    addAndMakeVisible(*titleComponent);

    modeLabel.setText("mode", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modeLabel);

    modeButton.setClickingTogglesState(true);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "ENVMODE", modeButton);
    modeButton.onClick = [this]() { updateModeLabel(); };
    addAndMakeVisible(modeButton);

    modeValueLabel.setJustificationType(juce::Justification::centred);
    modeValueLabel.setColour(juce::Label::textColourId, MyColours::valueTextColour);
    addAndMakeVisible(modeValueLabel);

    HelperFunctions::configureSlider(colorSlider);
    colorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "COLOR", colorSlider);
    colorSlider.textFromValueFunction = [](double v) {
        return juce::String::formatted("%.2f", v);
    };
    colorSlider.valueFromTextFunction = [](const juce::String& t) {
        return t.retainCharacters("0123456789.").getDoubleValue();
    };
    colorSlider.updateText();
    addAndMakeVisible(colorSlider);

    colorLabel.setText("color", juce::dontSendNotification);
    colorLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(colorLabel);

    updateModeLabel();
}

void EnvelopeComponent::resized()
{
    auto bounds = getLocalBounds();
    titleComponent->setBounds(bounds.removeFromTop(bounds.getHeight() * 0.12f));

    // Left half: mode toggle (same layout as link toggle)
    auto modeBounds = bounds.removeFromLeft(bounds.getWidth() * 0.50f);
    modeLabel.setBounds(modeBounds.removeFromTop(modeBounds.getHeight() * 0.15f));
    auto modeBtn = modeBounds.reduced(modeBounds.getWidth() * 0.30f);
    modeButton.setBounds(modeBtn.withCentre(juce::Point<int>(modeBtn.getCentreX(), modeBounds.getCentreY() - 20)));
    modeValueLabel.setBounds(juce::Rectangle<int>(
        modeBounds.getX(), modeBounds.getBottom() - 45, modeBounds.getWidth(), 20
    ));

    // Right half: color knob
    colorLabel.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.15f));
    colorSlider.setBounds(bounds.withTop(bounds.getY() - 20));
}

void EnvelopeComponent::updateModeLabel()
{
    modeValueLabel.setText(modeButton.getToggleState() ? "arithmetic" : "logarithmic",
                           juce::dontSendNotification);
}


// ===============================================================================================


DecayTiltComponent::DecayTiltComponent(juce::AudioProcessorValueTreeState& apvts)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("decay");
    addAndMakeVisible(*titleComponent);

    HelperFunctions::configureSlider(decaySlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DECAY", decaySlider);
    HelperFunctions::configureDelaySlider(decaySlider);
    decaySlider.updateText();
    addAndMakeVisible(decaySlider);

    decayLabel.setText("time", juce::dontSendNotification);
    decayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(decayLabel);

    HelperFunctions::configureSlider(tiltSlider);
    tiltAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "TILT", tiltSlider);
    tiltSlider.textFromValueFunction = [](double v) {
        return juce::String::formatted("%.2f", v);
    };
    tiltSlider.valueFromTextFunction = [](const juce::String& t) {
        return t.retainCharacters("-0123456789.").getDoubleValue();
    };
    tiltSlider.updateText();
    addAndMakeVisible(tiltSlider);

    tiltLabel.setText("tilt", juce::dontSendNotification);
    tiltLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tiltLabel);

}

DecayTiltComponent::~DecayTiltComponent()
{
}

void DecayTiltComponent::resized()
{
    auto bounds = getLocalBounds();

    auto titleBounds = bounds.removeFromTop(bounds.getHeight() * 0.12f);
    titleComponent->setBounds(titleBounds);

    auto timeBounds = bounds.removeFromLeft(bounds.getWidth() * 0.50f);
    auto timeLabelBounds = timeBounds.removeFromTop(timeBounds.getHeight() * 0.15f);
    decayLabel.setBounds(timeLabelBounds);
    decaySlider.setBounds(timeBounds.withTop(timeBounds.getY() - 20));

    auto tiltBounds = bounds;
    auto tiltLabelBounds = tiltBounds.removeFromTop(tiltBounds.getHeight() * 0.15f);
    tiltLabel.setBounds(tiltLabelBounds);
    tiltSlider.setBounds(tiltBounds.withTop(tiltBounds.getY() - 20));
}


// ===============================================================================================


ShiftDriftComponent::ShiftDriftComponent(juce::AudioProcessorValueTreeState& apvts,
                                         FDverbAudioProcessor& p)
    : m_apvts(apvts), audioProcessor(p)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("shift");
    addAndMakeVisible(*titleComponent);

    HelperFunctions::configureSlider(shiftSlider);
    shiftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "SHIFT", shiftSlider);
    shiftSlider.textFromValueFunction = [](double v) {
        return juce::String(static_cast<int>(std::round(v))) + " bins";
    };
    shiftSlider.valueFromTextFunction = [](const juce::String& t) {
        return static_cast<double>(static_cast<int>(std::round(
            t.retainCharacters("-0123456789").getDoubleValue())));
    };
    shiftSlider.updateText();
    shiftSlider.onValueChange = [this]() {
        const double halfBlock = audioProcessor.getCurrentFftSize() / 2.0;
        if (shiftSlider.getValue() > halfBlock)
            shiftSlider.setValue(halfBlock, juce::sendNotificationSync);
        else if (shiftSlider.getValue() < -halfBlock)
            shiftSlider.setValue(-halfBlock, juce::sendNotificationSync);
        updateSecondaryLabels();
    };
    addAndMakeVisible(shiftSlider);

    shiftLabel.setText("shift", juce::dontSendNotification);
    shiftLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(shiftLabel);

    shiftHzLabel.setJustificationType(juce::Justification::centred);
    shiftHzLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    shiftHzLabel.setColour(juce::Label::textColourId, MyColours::valueTextColour.withAlpha(0.55f));
    addAndMakeVisible(shiftHzLabel);

    HelperFunctions::configureSlider(driftSlider);
    driftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DRIFT", driftSlider);
    driftSlider.textFromValueFunction = [](double v) {
        return juce::String(static_cast<int>(std::round(v))) + " b/hop";
    };
    driftSlider.valueFromTextFunction = [](const juce::String& t) {
        return static_cast<double>(static_cast<int>(std::round(
            t.retainCharacters("-0123456789").getDoubleValue())));
    };
    driftSlider.updateText();
    driftSlider.onValueChange = [this]() {
        const double halfBlock = audioProcessor.getCurrentFftSize() / 2.0;
        if (driftSlider.getValue() > halfBlock)
            driftSlider.setValue(halfBlock, juce::sendNotificationSync);
        else if (driftSlider.getValue() < -halfBlock)
            driftSlider.setValue(-halfBlock, juce::sendNotificationSync);
        updateSecondaryLabels();
    };
    addAndMakeVisible(driftSlider);

    driftLabel.setText("drift", juce::dontSendNotification);
    driftLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(driftLabel);

    driftHzSLabel.setJustificationType(juce::Justification::centred);
    driftHzSLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    driftHzSLabel.setColour(juce::Label::textColourId, MyColours::valueTextColour.withAlpha(0.55f));
    addAndMakeVisible(driftHzSLabel);

    apvts.addParameterListener("FFTSIZE", this);
    updateSecondaryLabels();
}

ShiftDriftComponent::~ShiftDriftComponent()
{
    m_apvts.removeParameterListener("FFTSIZE", this);
}

void ShiftDriftComponent::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync(
        [safeThis = juce::Component::SafePointer<ShiftDriftComponent>(this)]() {
            if (safeThis) {
                safeThis->clampShiftDrift();       // may trigger onValueChange → updateSecondaryLabels
                safeThis->updateSecondaryLabels(); // covers case where no clamping occurred
            }
        });
}

void ShiftDriftComponent::clampShiftDrift()
{
    const double halfBlock = audioProcessor.getCurrentFftSize() / 2.0;
    if (shiftSlider.getValue() > halfBlock)
        shiftSlider.setValue(halfBlock, juce::sendNotificationSync);
    else if (shiftSlider.getValue() < -halfBlock)
        shiftSlider.setValue(-halfBlock, juce::sendNotificationSync);

    if (driftSlider.getValue() > halfBlock)
        driftSlider.setValue(halfBlock, juce::sendNotificationSync);
    else if (driftSlider.getValue() < -halfBlock)
        driftSlider.setValue(-halfBlock, juce::sendNotificationSync);
}

void ShiftDriftComponent::updateSecondaryLabels()
{
    const double sr = audioProcessor.getSampleRate();
    const int blockSize = audioProcessor.getCurrentFftSize();
    if (sr <= 0.0 || blockSize <= 0)
    {
        shiftHzLabel.setText("", juce::dontSendNotification);
        driftHzSLabel.setText("", juce::dontSendNotification);
        return;
    }

    const double binHz = sr / static_cast<double>(blockSize);

    auto formatHz = [](double hz, const juce::String& unit, const juce::String& kiloUnit) -> juce::String {
        if (std::abs(hz) >= 1000.0)
            return juce::String(hz / 1000.0, 2) + " " + kiloUnit;
        return juce::String(hz, 1) + " " + unit;
    };

    shiftHzLabel.setText(formatHz(shiftSlider.getValue() * binHz, "Hz", "kHz"),
                         juce::dontSendNotification);
    driftHzSLabel.setText(formatHz(driftSlider.getValue() * binHz * binHz / 1000, "Hz/ms", "kHz/ms"),
                          juce::dontSendNotification);
}

void ShiftDriftComponent::resized()
{
    auto bounds = getLocalBounds();

    auto titleBounds = bounds.removeFromTop(bounds.getHeight() * 0.12f);
    titleComponent->setBounds(titleBounds);

    auto shiftBounds = bounds.removeFromLeft(bounds.getWidth() * 0.50f);
    auto shiftLabelBounds = shiftBounds.removeFromTop(shiftBounds.getHeight() * 0.15f);
    shiftLabel.setBounds(shiftLabelBounds);
    auto shiftSliderBounds = shiftBounds.withTop(shiftBounds.getY() - 20);
    shiftSlider.setBounds(shiftSliderBounds);
    // The LookAndFeel translates the textbox up 25px, leaving 25px free at the slider bottom
    shiftHzLabel.setBounds(shiftSliderBounds.getX(), shiftSliderBounds.getBottom() - 23,
                           shiftSliderBounds.getWidth(), 16);

    auto driftBounds = bounds;
    auto driftLabelBounds = driftBounds.removeFromTop(driftBounds.getHeight() * 0.15f);
    driftLabel.setBounds(driftLabelBounds);
    auto driftSliderBounds = driftBounds.withTop(driftBounds.getY() - 20);
    driftSlider.setBounds(driftSliderBounds);
    driftHzSLabel.setBounds(driftSliderBounds.getX(), driftSliderBounds.getBottom() - 23,
                            driftSliderBounds.getWidth(), 16);
}


// ===============================================================================================


LinkWidthComponent::LinkWidthComponent(juce::AudioProcessorValueTreeState& apvts)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("stereo");
    addAndMakeVisible(*titleComponent);

    linkButton.setClickingTogglesState(true);
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "LINK", linkButton);
    linkButton.onClick = [this]() {
        linkValueLabel.setText(linkButton.getToggleState() ? "on" : "off", juce::dontSendNotification);
    };
    addAndMakeVisible(linkButton);

    linkValueLabel.setText(linkButton.getToggleState() ? "on" : "off", juce::dontSendNotification);
    linkValueLabel.setJustificationType(juce::Justification::centred);
    linkValueLabel.setColour(juce::Label::textColourId, MyColours::valueTextColour);
    addAndMakeVisible(linkValueLabel);

    linkLabel.setText("link", juce::dontSendNotification);
    linkLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(linkLabel);

    HelperFunctions::configureSlider(widthSlider);
    widthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 110, 20);
    widthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "WIDTH", widthSlider);
    widthSlider.textFromValueFunction = [](double v) -> juce::String {
        const double mg = v <= 1.0 ? 1.0 : 2.0 - v;
        const double sg = v <= 1.0 ? v : 1.0;
        return juce::String::formatted("%.1f M + %.1f S", mg, sg);
    };
    widthSlider.valueFromTextFunction = [](const juce::String& t) -> double {
        const juce::String lo = t.toLowerCase();
        const int mIdx = lo.indexOf("m");
        const int sIdx = lo.indexOf("s");
        if (mIdx >= 0 && sIdx > mIdx) {
            const double m = lo.substring(0, mIdx).retainCharacters("0123456789.").getDoubleValue();
            const double s = lo.substring(mIdx + 1).retainCharacters("0123456789.").getDoubleValue();
            // v ∈ [0,1]: M=1, S=v;  v ∈ [1,2]: S=1, M=2-v
            return juce::jlimit(0.0, 2.0, s > m ? 2.0 - m : s);
        }
        return juce::jlimit(0.0, 2.0, t.retainCharacters("0123456789.").getDoubleValue());
    };
    widthSlider.updateText();
    addAndMakeVisible(widthSlider);

    widthValueLabel.setJustificationType(juce::Justification::centred);
    widthValueLabel.setColour(juce::Label::textColourId, MyColours::valueTextColour);
    widthValueLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(widthValueLabel);

    widthLabel.setText("width", juce::dontSendNotification);
    widthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(widthLabel);
}

LinkWidthComponent::~LinkWidthComponent()
{
}

void LinkWidthComponent::resized()
{
    auto bounds = getLocalBounds();

    auto titleBounds = bounds.removeFromTop(bounds.getHeight() * 0.12f);
    titleComponent->setBounds(titleBounds);

    auto linkBounds = bounds.removeFromLeft(bounds.getWidth() * 0.50f);
    auto linkLabelBounds = linkBounds.removeFromTop(linkBounds.getHeight() * 0.15f);
    linkLabel.setBounds(linkLabelBounds);
    auto linkBtn = linkBounds.reduced(linkBounds.getWidth() * 0.30f);
    linkButton.setBounds(linkBtn.withCentre(juce::Point<int>(linkBtn.getCentreX(), linkBounds.getCentreY() - 20)));

    linkValueLabel.setBounds(juce::Rectangle<int>(
        linkBounds.getX(), linkBounds.getBottom() - 45,
        linkBounds.getWidth(), 20
    ));

    auto widthBounds = bounds;
    auto widthLabelBounds = widthBounds.removeFromTop(widthBounds.getHeight() * 0.15f);
    widthLabel.setBounds(widthLabelBounds);
    auto widthSliderBounds = widthBounds.withTop(widthBounds.getY() - 20);
    widthSlider.setBounds(widthSliderBounds);
    // Match position of the text box: LookAndFeel translates it up 25px from natural bottom
    widthValueLabel.setBounds(widthSliderBounds.getX(), widthSliderBounds.getBottom() - 45,
                              widthSliderBounds.getWidth(), 20);
}


// ===============================================================================================


BrickEqComponent::BrickEqComponent(juce::AudioProcessorValueTreeState& apvts)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("brick eq");
    addAndMakeVisible(*titleComponent);

    HelperFunctions::configureSlider(lowcutSlider);
    lowcutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "LOWCUT", lowcutSlider);
    HelperFunctions::configureFrequencySlider(lowcutSlider);
    lowcutSlider.updateText();
    addAndMakeVisible(lowcutSlider);

    lowcutLabel.setText("low cut", juce::dontSendNotification);
    lowcutLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lowcutLabel);

    HelperFunctions::configureSlider(highcutSlider);
    highcutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "HIGHCUT", highcutSlider);
    HelperFunctions::configureFrequencySlider(highcutSlider);
    highcutSlider.updateText();
    addAndMakeVisible(highcutSlider);

    highcutLabel.setText("high cut", juce::dontSendNotification);
    highcutLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(highcutLabel);

    lowcutSlider.onValueChange = [this]() {
        if (lowcutSlider.getValue() > highcutSlider.getValue())
            highcutSlider.setValue(lowcutSlider.getValue(), juce::sendNotificationSync);
    };

    highcutSlider.onValueChange = [this]() {
        if (highcutSlider.getValue() < lowcutSlider.getValue())
            lowcutSlider.setValue(highcutSlider.getValue(), juce::sendNotificationSync);
    };
}

BrickEqComponent::~BrickEqComponent()
{
}

void BrickEqComponent::resized()
{
    auto bounds = getLocalBounds();

    auto titleBounds = bounds.removeFromTop(bounds.getHeight() * 0.12f);
    titleComponent->setBounds(titleBounds);

    auto lowcutBounds = bounds.removeFromLeft(bounds.getWidth() * 0.50f);
    auto lowcutLabelBounds = lowcutBounds.removeFromTop(lowcutBounds.getHeight() * 0.15f);
    lowcutLabel.setBounds(lowcutLabelBounds);
    lowcutSlider.setBounds(lowcutBounds.withTop(lowcutBounds.getY() - 20));

    auto highcutBounds = bounds;
    auto highcutLabelBounds = highcutBounds.removeFromTop(highcutBounds.getHeight() * 0.15f);
    highcutLabel.setBounds(highcutLabelBounds);
    highcutSlider.setBounds(highcutBounds.withTop(highcutBounds.getY() - 20));
}


// ===============================================================================================


ErPreTailComponent::ErPreTailComponent(juce::AudioProcessorValueTreeState& apvts, FDverbAudioProcessor& p) :
    audioProcessor(p)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("er");
    addAndMakeVisible(*titleComponent);

    HelperFunctions::configureSlider(erTapsSlider);
    erTapsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "ERTAPS", erTapsSlider);
    erTapsSlider.textFromValueFunction = [](double v) {
        return juce::String(static_cast<int>(std::round(v)));
    };
    erTapsSlider.valueFromTextFunction = [](const juce::String& t) {
        return static_cast<double>(static_cast<int>(std::round(
            t.retainCharacters("0123456789").getDoubleValue())));
    };
    erTapsSlider.updateText();
    erTapsSlider.setColour(FdverbLookAndFeel::elementFillColourId, MyColours::erFillColour);
    addAndMakeVisible(erTapsSlider);

    erTapsLabel.setText("echoes", juce::dontSendNotification);
    erTapsLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(erTapsLabel);

    HelperFunctions::configureSlider(predelaySlider);
    predelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "PREDELAY", predelaySlider);
    HelperFunctions::configureDelaySlider(predelaySlider);
    predelaySlider.updateText();
    predelaySlider.setColour(FdverbLookAndFeel::elementFillColourId, MyColours::erFillColour);
    addAndMakeVisible(predelaySlider);

    predelaySlider.onValueChange = [this]() {
        double minTail = predelaySlider.getValue() + 1.0;
        if (tailDelaySlider.getValue() < minTail)
            tailDelaySlider.setValue(minTail, juce::sendNotificationSync);
    };

    predelayLabel.setText("pre delay", juce::dontSendNotification);
    predelayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(predelayLabel);

    HelperFunctions::configureSlider(tailDelaySlider);
    tailDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "TAILDELAY", tailDelaySlider);
    HelperFunctions::configureDelaySlider(tailDelaySlider);
    tailDelaySlider.updateText();
    tailDelaySlider.setColour(FdverbLookAndFeel::elementFillColourId, MyColours::erFillColour);
    addAndMakeVisible(tailDelaySlider);

    tailDelaySlider.onValueChange = [this]() {
        double floor = audioProcessor.getMinTailDelayMs();
        if (tailDelaySlider.getValue() < floor)
            tailDelaySlider.setValue(floor, juce::sendNotificationSync);
        double maxPreDelay = tailDelaySlider.getValue() - 1.0;
        if (predelaySlider.getValue() > maxPreDelay)
            predelaySlider.setValue(maxPreDelay, juce::sendNotificationSync);
    };

    tailDelayLabel.setText("tail delay", juce::dontSendNotification);
    tailDelayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tailDelayLabel);
}

ErPreTailComponent::~ErPreTailComponent()
{
}

void ErPreTailComponent::paint(juce::Graphics& g)
{
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold));
    g.setColour(MyColours::accentColour.withAlpha(0.6f));
    g.drawFittedText("noise seed", m_predelaySubtitleBounds, juce::Justification::centredBottom, 1);
}

void ErPreTailComponent::resized()
{
    auto bounds = getLocalBounds();

    auto titleBounds = bounds.removeFromTop(bounds.getHeight() * 0.12f);
    titleComponent->setBounds(titleBounds);

    auto erTapsBounds = bounds.removeFromLeft(bounds.getWidth() * 0.33f);
    auto erTapsLabelBounds = erTapsBounds.removeFromTop(erTapsBounds.getHeight() * 0.15f);
    erTapsLabel.setBounds(erTapsLabelBounds);
    erTapsSlider.setBounds(erTapsBounds.withTop(erTapsBounds.getY() - 20));

    auto predelayBounds = bounds.removeFromLeft(bounds.getWidth() * 0.50f);
    auto predelayLabelBounds = predelayBounds.removeFromTop(predelayBounds.getHeight() * 0.15f);
    predelayLabel.setBounds(predelayLabelBounds.removeFromTop(juce::roundToInt(predelayLabelBounds.getHeight() * 0.55f)));
    m_predelaySubtitleBounds = predelayLabelBounds.expanded(0, 4);
    predelaySlider.setBounds(predelayBounds.withTop(predelayBounds.getY() - 20));

    auto tailDelayBounds = bounds;
    auto tailDelayLabelBounds = tailDelayBounds.removeFromTop(tailDelayBounds.getHeight() * 0.15f);
    tailDelayLabel.setBounds(tailDelayLabelBounds);
    tailDelaySlider.setBounds(tailDelayBounds.withTop(tailDelayBounds.getY() - 20));
}


// ===============================================================================================


MixComponent::MixComponent(juce::AudioProcessorValueTreeState& apvts)
{
    titleComponent = std::make_unique<TitleWithLinesComponent>("mix");
    addAndMakeVisible(*titleComponent);

    HelperFunctions::configureSlider(drySlider);
    dryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DRY", drySlider);
    HelperFunctions::configureGainSlider(drySlider);
    drySlider.updateText();
    drySlider.setColour(FdverbLookAndFeel::elementFillColourId, MyColours::dryFillColour);
    addAndMakeVisible(drySlider);

    dryLabel.setText("dry", juce::dontSendNotification);
    dryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dryLabel);

    HelperFunctions::configureSlider(erLevelSlider);
    erLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "ERLEVEL", erLevelSlider);
    HelperFunctions::configureGainSlider(erLevelSlider);
    erLevelSlider.updateText();
    erLevelSlider.setColour(FdverbLookAndFeel::elementFillColourId, MyColours::erFillColour);
    addAndMakeVisible(erLevelSlider);

    erLevelLabel.setText("er level", juce::dontSendNotification);
    erLevelLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(erLevelLabel);

    HelperFunctions::configureSlider(tailSlider);
    tailAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "TAIL", tailSlider);
    HelperFunctions::configureGainSlider(tailSlider);
    tailSlider.updateText();
    addAndMakeVisible(tailSlider);

    tailLabel.setText("tail level", juce::dontSendNotification);
    tailLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tailLabel);
}

MixComponent::~MixComponent()
{
}

void MixComponent::resized()
{
    auto bounds = getLocalBounds();

    auto titleBounds = bounds.removeFromTop(bounds.getHeight() * 0.12f);
    titleComponent->setBounds(titleBounds);

    auto tailBounds = bounds.removeFromLeft(bounds.getWidth() * 0.33f);
    auto tailLabelBounds = tailBounds.removeFromTop(tailBounds.getHeight() * 0.15f);
    tailLabel.setBounds(tailLabelBounds);
    tailSlider.setBounds(tailBounds.withTop(tailBounds.getY() - 20));

    auto erLevelBounds = bounds.removeFromLeft(bounds.getWidth() * 0.50f);
    auto erLevelLabelBounds = erLevelBounds.removeFromTop(erLevelBounds.getHeight() * 0.15f);
    erLevelLabel.setBounds(erLevelLabelBounds);
    erLevelSlider.setBounds(erLevelBounds.withTop(erLevelBounds.getY() - 20));

    auto dryBounds = bounds;
    auto dryLabelBounds = dryBounds.removeFromTop(dryBounds.getHeight() * 0.15f);
    dryLabel.setBounds(dryLabelBounds);
    drySlider.setBounds(dryBounds.withTop(dryBounds.getY() - 20));
}


// ===============================================================================================


// ===============================================================================================


MainUI::MainUI(FDverbAudioProcessor& p) : audioProcessor(p)
{
    setLookAndFeel(&fdverbLookAndFeel);

    titleComponent = std::make_unique<FDverbTitleComponent>();
    addAndMakeVisible(*titleComponent);

    freezeTitleComponent = std::make_unique<FreezeTitleComponent>(p.apvts);
    addAndMakeVisible(*freezeTitleComponent);

    ethLogoComponent = std::make_unique<EthLogoComponent>();
    addAndMakeVisible(*ethLogoComponent);

    spectralDisplay = std::make_unique<SpectralDisplay>(audioProcessor);
    addAndMakeVisible(*spectralDisplay);

    fftsizeComponent = std::make_unique<FFTSizeComponent>(p.apvts, p);
    addAndMakeVisible(*fftsizeComponent);

    envelopeComponent = std::make_unique<EnvelopeComponent>(p.apvts);
    addAndMakeVisible(*envelopeComponent);

    decayTiltComponent = std::make_unique<DecayTiltComponent>(p.apvts);
    addAndMakeVisible(*decayTiltComponent);

    linkWidthComponent = std::make_unique<LinkWidthComponent>(p.apvts);
    addAndMakeVisible(*linkWidthComponent);

    shiftDriftComponent = std::make_unique<ShiftDriftComponent>(p.apvts, p);
    addAndMakeVisible(*shiftDriftComponent);

    brickEqComponent = std::make_unique<BrickEqComponent>(p.apvts);
    addAndMakeVisible(*brickEqComponent);

    erPreTailComponent = std::make_unique<ErPreTailComponent>(p.apvts, p);
    addAndMakeVisible(*erPreTailComponent);

    mixComponent = std::make_unique<MixComponent>(p.apvts);
    addAndMakeVisible(*mixComponent);

    versionLabel.setText("v" PLUGIN_VERSION, juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);
    versionLabel.setFont(juce::Font(10.0f));
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(versionLabel);

    if constexpr (kShowDevInfo) {
        buildIdLabel.setText(juce::String(__DATE__) + " " + __TIME__, juce::dontSendNotification);
        buildIdLabel.setJustificationType(juce::Justification::centredLeft);
        buildIdLabel.setFont(juce::Font(10.0f));
        buildIdLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(buildIdLabel);

        effectiveFftSizeLabel.setText("fft size: " + juce::String(audioProcessor.getCurrentFftSize()), juce::dontSendNotification);
        effectiveFftSizeLabel.setJustificationType(juce::Justification::centredLeft);
        effectiveFftSizeLabel.setFont(juce::Font(10.0f));
        effectiveFftSizeLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(effectiveFftSizeLabel);
    }

    startTimerHz(10);
}

MainUI::~MainUI()
{
    setLookAndFeel(nullptr);
}

void MainUI::paint(juce::Graphics& g)
{
    g.fillAll(MyColours::windowBGColour);
}

void MainUI::resized()
{
    auto bounds = getLocalBounds();
    const int totalW = bounds.getWidth();
    const int vizMargin = 15;

    // Title bar: FDverb title | freeze controls | ETH logo
    // Proportions mirror the grid columns below (30 / 28 / 42)
    auto topSection = bounds.removeFromTop(bounds.getHeight() * 0.15f);
    auto titleArea  = topSection.removeFromLeft(juce::roundToInt(totalW * 0.30f));
    auto freezeArea = topSection.removeFromLeft(juce::roundToInt(topSection.getWidth() * 0.50f));
    // topSection remainder = ETH logo area

    auto versionBounds = titleArea.removeFromBottom(14);
    titleComponent->setBounds(titleArea.reduced(8, 5));
    versionLabel.setBounds(versionBounds.reduced(8, 0));
    freezeTitleComponent->setBounds(freezeArea.reduced(8, 5));
    ethLogoComponent->setBounds(topSection.removeFromLeft(topSection.getWidth() * 0.78f).expanded(30, 0));

    if constexpr (kShowDevInfo) {
        buildIdLabel.setBounds(titleArea.removeFromBottom(16).reduced(8, 0));
        effectiveFftSizeLabel.setBounds(titleArea.removeFromBottom(16).reduced(8, 0));
    }

    // Main grid: left / middle / right  (same column proportions as title bar)
    auto leftSection   = bounds.removeFromLeft(bounds.getWidth() * 0.30f);
    auto middleSection = bounds.removeFromLeft(bounds.getWidth() * 0.40f);
    auto rightSection  = bounds;

    // Left column: block size (top), decay (mid), stereo (bottom)
    auto blockSizeSlot = leftSection.removeFromTop(leftSection.getHeight() * 0.33f);
    auto decaySlot     = leftSection.removeFromTop(leftSection.getHeight() * 0.50f);
    auto stereoSlot    = leftSection;
    fftsizeComponent->setBounds(blockSizeSlot);
    decayTiltComponent->setBounds(decaySlot);
    linkWidthComponent->setBounds(stereoSlot);

    // Middle column: envelope (top), shift (mid), brick eq (bottom)
    auto envelopeSlot  = middleSection.removeFromTop(middleSection.getHeight() * 0.33f);
    auto shiftSlot     = middleSection.removeFromTop(middleSection.getHeight() * 0.50f);
    auto brickEqSlot   = middleSection;
    envelopeComponent->setBounds(envelopeSlot);
    shiftDriftComponent->setBounds(shiftSlot);
    brickEqComponent->setBounds(brickEqSlot);

    // Right column: visualizer (top), er (mid), mix (bottom)
    auto visualizerSlot = rightSection.removeFromTop(rightSection.getHeight() * 0.33f);
    auto erSlot         = rightSection.removeFromTop(rightSection.getHeight() * 0.50f);
    auto mixSlot        = rightSection;
    spectralDisplay->setBounds(visualizerSlot.withTrimmedLeft(vizMargin)
                                             .withTrimmedRight(vizMargin)
                                             .withTrimmedBottom(vizMargin));
    erPreTailComponent->setBounds(erSlot);
    mixComponent->setBounds(mixSlot);
}

void MainUI::timerCallback()
{
    if constexpr (kShowDevInfo) {
        effectiveFftSizeLabel.setText("fft size: " + juce::String(audioProcessor.getCurrentFftSize()), juce::dontSendNotification);
    }
}
