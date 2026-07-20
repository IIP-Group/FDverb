// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "SpectralDisplay.h"
#include "FdverbLookAndFeel.h"
#include <BinaryData.h>

SpectralDisplay::SpectralDisplay(FDverbAudioProcessor& processor)
    : m_processor(processor)
{
    startTimerHz(30);
}

SpectralDisplay::~SpectralDisplay()
{
    stopTimer();
}

void SpectralDisplay::resized()
{
    auto bounds = getLocalBounds();
    if (bounds.getWidth() > 0 && bounds.getHeight() > 0) {
        m_spectrogram = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
        m_writePos = 0;

        // Paint logo into the initial empty image — it scrolls out naturally as audio fills in
        juce::Graphics g(m_spectrogram);
        juce::Image logo = juce::ImageCache::getFromMemory(BinaryData::FDverb_Logo_White_png,
                                                           BinaryData::FDverb_Logo_White_pngSize);
        if (logo.isValid()) {
            g.setOpacity(0.75f);
            auto area = bounds.reduced(8, 50);
            g.drawImageWithin(logo, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                              juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid |
                              juce::RectanglePlacement::onlyReduceInSize);
        } else {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::bold));
            g.drawFittedText("FDverb", bounds.withTrimmedLeft(bounds.getWidth() / 2),
                             juce::Justification::centredRight, 1);
        }
    }
}

void SpectralDisplay::mouseUp(const juce::MouseEvent& /*event*/)
{
    switch (m_displayState)
    {
        case DisplayState::GrayMap:  m_displayState = DisplayState::ColorMap; break;
        case DisplayState::ColorMap: m_displayState = DisplayState::Off; break;
        case DisplayState::Off:
            m_displayState = DisplayState::GrayMap;
            m_spectrogram.clear(m_spectrogram.getBounds(), juce::Colours::black);
            break;
    }
    repaint();
}

void SpectralDisplay::timerCallback()
{
    if (m_spectrogram.isNull())
        return;

    if (m_displayState == DisplayState::Off)
        return;

    int width = m_spectrogram.getWidth();
    int height = m_spectrogram.getHeight();

    // Shift entire image left by one pixel
    m_spectrogram.moveImageSection(0, 0, 1, 0, width - 1, height);

    // Write new column at the right edge
    int x = width - 1;

    // Compute cutoff positions before pixel loop so we can dim cut regions
    const double sr = m_processor.getSampleRate();
    const int blockSize = m_processor.getCurrentFftSize();
    const float lowCutHz  = *m_processor.apvts.getRawParameterValue("LOWCUT");
    const float highCutHz = *m_processor.apvts.getRawParameterValue("HIGHCUT");

    // Only draw/dim when not at the default (fully-open) values
    const bool drawLow  = (sr > 0 && blockSize > 0 && lowCutHz  > 20.5f);
    const bool drawHigh = (sr > 0 && blockSize > 0 && highCutHz < 19990.0f);

    int lowY = height, highY = -1;  // defaults: no dimming
    if (drawLow || drawHigh) {
        // envelopeDisplay[i] covers FFT bins [i*envelopeSize/N .. (i+1)*envelopeSize/N].
        // The display maps y linearly to display bins 0..kNumDisplayBins*0.8-1,
        // so hzToY must convert Hz → FFT bin → display bin → proportion.
        const int envelopeSize = blockSize / 2 + 1;
        const float maxDispBin = (float)(FDverbAudioProcessor::kNumDisplayBins) * 0.8f - 1.0f;
        const float maxDisplayHz = maxDispBin
                                   * (float)envelopeSize / (float)(FDverbAudioProcessor::kNumDisplayBins)
                                   * (float)sr / (float)blockSize;
        auto hzToY = [&](float hz) -> int {
            float proportion = juce::jlimit(0.0f, 1.0f, hz / maxDisplayHz);
            return height - 1 - (int)(proportion * (float)(height - 1));
        };
        if (drawLow)  lowY  = hzToY(lowCutHz);
        if (drawHigh) highY = hzToY(highCutHz);
    }

    const float maxBinF = (float)(FDverbAudioProcessor::kNumDisplayBins) * 0.8f - 1.0f;
    const int   maxBin  = juce::roundToInt(maxBinF);

    for (int y = 0; y < height; ++y) {
        // Map y to a fractional bin position and interpolate between neighbours
        float binF = juce::jmap((float)(height - 1 - y), 0.0f, (float)(height - 1), 0.0f, maxBinF);
        int   bin  = juce::jmin((int)(binF + 0.5f), maxBin);

        float value = m_processor.envelopeDisplay[bin].load();

        float dB = juce::Decibels::gainToDecibels(value, -60.0f);
        float normalized = juce::jmap(dB, -60.0f, 0.0f, 0.0f, 1.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);

        juce::Colour colour = getColourForValue(normalized);

        // Dim frequencies outside the pass band (applied to final colour, not normalized,
        // so the colour map hue is preserved in colour mode)
        if ((drawLow && y > lowY) || (drawHigh && y < highY))
            colour = colour.withMultipliedBrightness(0.4f);

        m_spectrogram.setPixelAt(x, y, colour);
    }

    // Draw cutoff line markers on top of the dimmed spectrum
    const juce::Colour cutColour = MyColours::accentColour.withAlpha(0.9f);
    if (drawLow  && lowY  >= 0 && lowY  < height) m_spectrogram.setPixelAt(x, lowY,  cutColour);
    if (drawHigh && highY >= 0 && highY < height) m_spectrogram.setPixelAt(x, highY, cutColour);

    repaint();
}

void SpectralDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(MyColours::envGramBGColour);
    g.fillRect(bounds);

    // Draw spectrogram
    if (m_displayState != DisplayState::Off && !m_spectrogram.isNull())
        g.drawImageAt(m_spectrogram, 0, 0);

    // Border
    g.setColour(MyColours::accentColour.withAlpha(0.5f));
    g.drawRect(bounds, 1.0f);

    // Label
    g.setColour(MyColours::accentColour.withAlpha(0.6f));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold));
    juce::String label = (m_displayState == DisplayState::Off) ? "envelope-gram off" : "envelope-gram";
    g.drawText(label, bounds.reduced(5), juce::Justification::topRight);
}

//juce::Colour SpectralDisplay::getColourForValue(float normalized)
//{
//    if (normalized < 0.01f)
//        return juce::Colour(0xff0a0a0a);  // background for silence
//
//    // Black -> purple -> magenta -> white
//    if (normalized < 0.33f)
//    {
//        float t = normalized / 0.33f;
//        return juce::Colour(0xff0a0a0a).interpolatedWith(juce::Colour(0xffaa00ff), t);
//    }
//    else if (normalized < 0.66f)
//    {
//        float t = (normalized - 0.33f) / 0.33f;
//        return juce::Colour(0xffaa00ff).interpolatedWith(juce::Colour(0xffff00ff), t);
//    }
//    else
//    {
//        float t = (normalized - 0.66f) / 0.34f;
//        return juce::Colour(0xffff00ff).interpolatedWith(juce::Colours::white, t);
//    }
//}

juce::Colour SpectralDisplay::getColourForValue(float normalized)
{
    if (normalized < 0.01f)
        return juce::Colour(0xff0a0a0a);

    // Black -> dark blue -> cyan -> green -> yellow -> red -> white
    const float stops[] =     { 0.0f,  0.15f,  0.3f,   0.45f,  0.6f,   0.8f,   1.0f };
    
//    const juce::Colour colorMap[] = {
//        juce::Colour(0xff000020),  // near black
//        juce::Colour(0xff0000cc),  // dark blue
//        juce::Colour(0xff00cccc),  // cyan
//        juce::Colour(0xff00cc00),  // green
//        juce::Colour(0xffcccc00),  // yellow
//        juce::Colour(0xffff0000),  // red
//        juce::Colours::white
//    };
    const juce::Colour colorMap[] = {
        juce::Colour(0xff000000),
        juce::Colour(0xff081c45),
        juce::Colour(0xff1d4fb4),
        juce::Colour(0xff861bbb),
        juce::Colour(0xffcf1797),
        juce::Colour(0xfff547c1),
        juce::Colour(0xffffffff)
    };

    const juce::Colour grayMap[] = {
        juce::Colours::black,
        juce::Colour(0xff111111),
        juce::Colour(0xff222222),
        juce::Colour(0xff444444),
        juce::Colour(0xff777777),
        juce::Colour(0xffaaaaaa),
        juce::Colours::white
    };

    const juce::Colour* colMap = (m_displayState == DisplayState::ColorMap) ? colorMap : grayMap;

    for (int i = 0; i < 6; ++i) {
        if (normalized <= stops[i + 1]) {
            float t = (normalized - stops[i]) / (stops[i + 1] - stops[i]);
            return colMap[i].interpolatedWith(colMap[i + 1], t);
        }
    }

    return juce::Colours::white;
}
