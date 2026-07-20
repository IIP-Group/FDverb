// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <JuceHeader.h>

struct MyColours
    {
        inline static const juce::Colour windowBGColour = juce::Colour(0xff0f1c20);
        inline static const juce::Colour elementBGColour = juce::Colour(0xff1a1a1a);
        inline static const juce::Colour envGramBGColour = juce::Colour(0xff0a0a0a);
        inline static const juce::Colour accentColour = juce::Colour(0xffffffff);
        inline static const juce::Colour accentColourAlternate = juce::Colour(0xffaaaaaa);
        inline static const juce::Colour valueTextColour = juce::Colour(0xffeeeeee);

        // Fill colours for knobs and toggle buttons, grouped by signal role:
        inline static const juce::Colour erFillColour   = juce::Colour(0xff383838);
        inline static const juce::Colour dryFillColour  = juce::Colour(0xff555555);
        inline static const juce::Colour tailFillColour = juce::Colour(0xff0a0a0a);
    };

class TitleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TitleLookAndFeel()
    {
        setColour(juce::Label::textColourId, MyColours::accentColour);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    }
    
    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold);
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitleLookAndFeel)
};

class FdverbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Colour ID for the filled body of knobs and toggle-button tracks.
    // Override per-component with setColour(FdverbLookAndFeel::elementFillColourId, ...).
    static constexpr int elementFillColourId = 0x10fd0001;

    FdverbLookAndFeel()
    {
        // Neon magenta/purple color scheme
        setColour(juce::Slider::thumbColourId, MyColours::accentColour);
        setColour(juce::Slider::rotarySliderFillColourId, MyColours::accentColourAlternate);
        setColour(juce::Slider::rotarySliderOutlineColourId, MyColours::elementBGColour);
        setColour(elementFillColourId, MyColours::tailFillColour);
        setColour(juce::Slider::textBoxTextColourId, MyColours::valueTextColour);

        setColour(juce::ComboBox::textColourId, MyColours::accentColour);
        setColour(juce::ComboBox::backgroundColourId, MyColours::elementBGColour);
        setColour(juce::ComboBox::arrowColourId, MyColours::accentColour);
        
        setColour(juce::Label::textColourId, MyColours::accentColour);
    }
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height);
        auto centre = bounds.getCentre();
        const float maxRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f * 0.6f - 10.0f;
        const float radius = juce::jmin(maxRadius, 22.0f);
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        
        // Glow effect
        for (int i = 3; i >= 1; --i) {
            g.setColour(MyColours::accentColourAlternate.withAlpha(0.1f / i));
            g.drawEllipse(centre.x - radius - i, centre.y - radius - i,
                         (radius + i) * 2.0f, (radius + i) * 2.0f, 2.0f);
        }
        
        // Knob body
        g.setColour(slider.findColour(elementFillColourId));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        
        // Outer ring
        g.setColour(juce::Colour(MyColours::accentColour).withAlpha(0.8f));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
        
        // Pointer
        auto thumbDistance = radius * 0.75f;
        juce::Point<float> thumbPoint(
            centre.x + thumbDistance * std::cos(toAngle - juce::MathConstants<float>::halfPi),
            centre.y + thumbDistance * std::sin(toAngle - juce::MathConstants<float>::halfPi)
        );
        
        g.setColour(MyColours::accentColour);
        g.drawLine(juce::Line<float>(centre, thumbPoint), 3.0f);
        
        // Center dot
        g.setColour(MyColours::accentColour);
        g.fillEllipse(centre.x - 3, centre.y - 3, 6, 6);
        
        // Value arc
        if (slider.isEnabled() && toAngle > rotaryStartAngle) {
            juce::Path arc;
            arc.addCentredArc(centre.x, centre.y, radius + 10, radius + 10,
                             0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(MyColours::accentColourAlternate);
            g.strokePath(arc, juce::PathStrokeType(3.0f));
        }
        
    }
    
    // void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
    //                      bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    // {
    //     auto bounds = button.getLocalBounds().toFloat();
    //     auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    //     auto buttonBounds = bounds.withSizeKeepingCentre(size * 0.8f, size * 0.8f);
    //
    //     // Background glow when enabled
    //     if (button.getToggleState()) {
    //         g.setColour(MyColours::accentColourAlternate.withAlpha(0.3f));
    //         g.fillEllipse(buttonBounds.expanded(4));
    //     }
    //
    //     // Button body
    //     g.setColour(MyColours::elementBGColour);
    //     g.fillEllipse(buttonBounds);
    //
    //     // Border
    //     g.setColour(button.getToggleState() ? MyColours::accentColour : MyColours::accentColour.darker());
    //     g.drawEllipse(buttonBounds, 2.0f);
    //
    //     // Center indicator when toggled
    //     if (button.getToggleState()) {
    //         auto centerBounds = buttonBounds.reduced(buttonBounds.getWidth() * 0.3f);
    //         g.setColour(MyColours::accentColour);
    //         g.fillEllipse(centerBounds);
    //     }
    // }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                         bool, bool) override
    {
        constexpr float toggleWidth  = 40.0f;
        constexpr float toggleHeight = 22.0f;
        constexpr float thumbInset   = 3.0f;

        const bool toggled = button.getToggleState();
        auto track = juce::Rectangle<float>(toggleWidth, toggleHeight)
                         .withCentre(button.getLocalBounds().toFloat().getCentre());

        // Track background
        g.setColour(button.findColour(elementFillColourId));
        g.fillRoundedRectangle(track, toggleHeight / 2.0f);

        // Track outline
        g.setColour(MyColours::accentColour);
        g.drawRoundedRectangle(track.reduced(0.5f), toggleHeight / 2.0f, 1.5f);

        // Thumb
        const float thumbDiameter = toggleHeight - 2.0f * thumbInset;
        const float thumbX = toggled ? track.getRight() - thumbInset - thumbDiameter
                                     : track.getX()    + thumbInset;
        g.setColour(MyColours::accentColour);
        g.fillEllipse(thumbX, track.getY() + thumbInset, thumbDiameter, thumbDiameter);
    }
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                                    sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const float trackThickness = 3.0f;
        const float trackY = y + (height - trackThickness) * 0.5f;

        // // Uniform track
        // g.setColour(MyColours::accentColour.withAlpha(0.45f));
        // g.fillRoundedRectangle((float)x, trackY, (float)width, trackThickness, trackThickness * 0.5f);

        // Faint tick marks at each discrete step
        const double range = slider.getMaximum() - slider.getMinimum();
        const double interval = slider.getInterval();
        if (interval > 0.0 && range / interval <= 32.0)
        {
            const int numSteps = juce::roundToInt(range / interval) + 1;
            g.setColour(MyColours::accentColour.withAlpha(0.45f));
            for (int i = 0; i < numSteps; ++i)
            {
                const double val = slider.getMinimum() + i * interval;
                const float norm = (float)slider.valueToProportionOfLength(val);
                const float tickX = (float)x + norm * (float)width;
                g.fillRect(juce::Rectangle<float>(tickX - 0.5f, trackY - 5.0f, 3.0f, trackThickness + 10.0f));
            }
        }

        // Thumb
        const float thumbW = 8.0f;
        const float thumbH = 22.0f;
        g.setColour(MyColours::accentColour);
        g.fillRoundedRectangle(sliderPos - thumbW * 0.5f, y + ((float)height - thumbH) * 0.5f,
                                thumbW, thumbH, 3.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                     int buttonX, int buttonY, int buttonW, int buttonH,
                     juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, width, height);
        
        // Background
        g.setColour(MyColours::elementBGColour);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(box.hasKeyboardFocus(true) ? MyColours::accentColour : MyColours::accentColour.darker());
        g.drawRoundedRectangle(bounds.reduced(1), 4.0f, 2.0f);
        
        // Arrow
        auto arrowBounds = juce::Rectangle<float>(buttonX, buttonY, buttonW, buttonH).reduced(buttonW * 0.3f, buttonH * 0.35f);
        juce::Path arrow;
        arrow.startNewSubPath(arrowBounds.getX(), arrowBounds.getY());
        arrow.lineTo(arrowBounds.getCentreX(), arrowBounds.getBottom());
        arrow.lineTo(arrowBounds.getRight(), arrowBounds.getY());
        
        g.setColour(MyColours::accentColour);
        g.strokePath(arrow, juce::PathStrokeType(2.0f));
    }
    
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        auto layout = juce::LookAndFeel_V4::getSliderLayout(slider);
        if (slider.getSliderStyle() == juce::Slider::Rotary ||
            slider.getSliderStyle() == juce::Slider::RotaryHorizontalDrag ||
            slider.getSliderStyle() == juce::Slider::RotaryVerticalDrag ||
            slider.getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
        {
            layout.textBoxBounds = layout.textBoxBounds.translated(0, -25);
        }
        return layout;
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool toggled = button.getToggleState();

        auto bgColour    = toggled ? MyColours::accentColour    : button.findColour(elementFillColourId);
        auto borderColour = toggled ? MyColours::elementBGColour : MyColours::accentColour;

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        const bool toggled = button.getToggleState();
        g.setColour(toggled ? MyColours::elementBGColour : MyColours::accentColour);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds(),
                         juce::Justification::centred, 1);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        return juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold);
    }
    
    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold);
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FdverbLookAndFeel)
};
