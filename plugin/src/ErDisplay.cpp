// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ErDisplay.h"
#include "FdverbLookAndFeel.h"
#include "Utils.h"

ErDisplay::ErDisplay(FDverbAudioProcessor& processor, const SpectralDisplay& envelopeGram)
    : m_processor(processor), m_envelopeGram(envelopeGram)
{
    // Clicks belong to the spectral display underneath, which owns the mode cycle — including the
    // ErPlot mode that holds this plot on screen. Passing them through means a click lands the same
    // way whether or not the plot happens to be up.
    setInterceptsMouseClicks(false, false);
    setAlpha(0.0f);
}

ErDisplay::~ErDisplay()
{
    stopTimer();
}

void ErDisplay::showTemporarily()
{
    updateFromParameters();

    if (m_constant) {
        // the plot stays up until the mode changes: no hold to restart and nothing to fade, so the
        // fresh taps are all there is to draw. "echoes off" is a state worth showing here.
        repaint();
        return;
    }

    if (m_numTaps == 0) {
        // the early reflections are off, so there is nothing to plot: let a visible plot fade out
        m_holdMsRemaining = 0.0f;
        repaint();

        if (isVisible() && !isTimerRunning())
            startTimerHz(kTimerHz);
        return;
    }

    m_holdMsRemaining = kHoldMs;

    setVisible(true);

    if (!isTimerRunning())
        startTimerHz(kTimerHz);

    repaint();
}

void ErDisplay::refreshIfVisible()
{
    if (isVisible())
        showTemporarily();
}

void ErDisplay::setConstant(bool shouldBeConstant)
{
    if (m_constant == shouldBeConstant)
        return;

    m_constant = shouldBeConstant;

    if (m_constant) {
        // Straight to fully on: the mode switch is the click's visible effect, so fading in here
        // would only lag behind it. The taps may have moved while the plot was away.
        updateFromParameters();
        stopTimer();
        m_holdMsRemaining = 0.0f;
        m_alpha = 1.0f;
        setVisible(true);
        setAlpha(m_alpha);
        repaint();
    }
    else {
        // fades out from wherever it is, unless a knob summons it back in the meantime
        m_holdMsRemaining = 0.0f;

        if (isVisible() && !isTimerRunning())
            startTimerHz(kTimerHz);
    }
}

void ErDisplay::timerCallback()
{
    const float frameMs = 1000.0f / (float)kTimerHz;
    const float step = frameMs / kFadeMs;

    if (m_constant) {
        stopTimer();  // constant mode is not animated
        return;
    }

    if (m_holdMsRemaining > 0.0f) {
        m_holdMsRemaining -= frameMs;
        m_alpha = juce::jmin(1.0f, m_alpha + step);
    }
    else {
        m_alpha = juce::jmax(0.0f, m_alpha - step);
    }

    setAlpha(m_alpha);  // also repaints

    // idle once fully faded out — the spectral display owns the canvas again
    if (m_alpha <= 0.0f && m_holdMsRemaining <= 0.0f) {
        setVisible(false);
        stopTimer();
    }
}

void ErDisplay::updateFromParameters()
{
    const float preDelayMs  = *m_processor.apvts.getRawParameterValue("PREDELAY");
    const float tailDelayMs = *m_processor.apvts.getRawParameterValue("TAILDELAY");
    const float width       = *m_processor.apvts.getRawParameterValue("WIDTH");
    const float erLevelDb   = *m_processor.apvts.getRawParameterValue("ERLEVEL");
    const int   numTaps     = (int)*m_processor.apvts.getRawParameterValue("ERTAPS");

    // before the first prepareToPlay the rate is still unknown; any value keeps the plot
    // self-consistent, since the same rate converts the tap delays back to milliseconds
    const double sampleRate = m_processor.getSampleRate() > 0.0 ? m_processor.getSampleRate() : 44100.0;
    const int numChannels = juce::jmax(1, m_processor.getTotalNumInputChannels());

    // the same clamps the processor applies, so the plot matches what is audible
    m_tapSampleRate = sampleRate;
    m_preDelayMs  = EarlyReflections::clampPreDelayMs(preDelayMs, tailDelayMs);
    m_tailDelayMs = tailDelayMs;

    // The tail cannot arrive before the FFT latency floor; the processor clamps the setting up to it
    // rather than rewriting the parameter, so the region starts wherever the tail is really audible.
    const float tailLevelDb = *m_processor.apvts.getRawParameterValue("TAIL");
    const float decayMs     = *m_processor.apvts.getRawParameterValue("DECAY");
    const float envMode     = *m_processor.apvts.getRawParameterValue("ENVMODE");
    const float freeze      = *m_processor.apvts.getRawParameterValue("FREEZE");

    m_tailLevelDb = tailLevelDb;
    m_decayMs = juce::jmax(1.0f, decayMs);
    m_arithmeticEnvelope = envMode > 0.5f;
    m_frozen = freeze > 0.5f;
    m_tailStartMs = juce::jmax(tailDelayMs, m_processor.getMinTailDelayMs());
    m_numTaps = EarlyReflections::computeTaps(preDelayMs, tailDelayMs, juce::jmin(width, 1.0f),
                                             numTaps, sampleRate, numChannels, m_taps.data());

    // The plot shows the taps as they are heard, so the ER level sets the height of the first tap
    // and slides every other tap down with it. dbToLinear mutes below -60 dB, which is the floor
    // the plot draws to anyway.
    const float erGain = dbToLinear(erLevelDb);
    for (int i = 0; i < m_numTaps; ++i) {
        m_taps[(size_t)i].gainL *= erGain;
        m_taps[(size_t)i].gainR *= erGain;
    }
}

float ErDisplay::tailLevelDbAt(float ms) const
{
    const float delta = ms - m_tailStartMs;
    if (delta < 0.0f)
        return kMinDb - 1.0f;

    if (m_frozen)
        return m_tailLevelDb;  // freeze holds the envelope, so it never decays

    if (m_arithmeticEnvelope) {
        // tOneToCoef subtracts 2/decay of full-scale amplitude per millisecond
        const float amplitude = 1.0f - 2.0f * delta / m_decayMs;
        if (amplitude <= 0.0f)
            return kMinDb - 1.0f;
        return m_tailLevelDb + juce::Decibels::gainToDecibels(amplitude, kMinDb - 1.0f);
    }

    // tSixtyToCoef makes the envelope lose exactly 60 dB per decay time
    return m_tailLevelDb - 60.0f * delta / m_decayMs;
}

void ErDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dims rather than hides the envelope-gram scrolling underneath. In constant mode the
    // spectrogram is idle, so there is nothing left worth showing through and the plot takes a
    // clean background instead.
    g.setColour(MyColours::envGramBGColour.withAlpha(m_constant ? 1.0f : kBackgroundAlpha));
    g.fillRect(bounds);

    // Plot area: a left gutter for the dB ticks, a bottom strip for the time ticks, and enough
    // head room that a dot sitting at 0 dB is not clipped.
    const float dbGutter = 28.0f;
    const float rightPad = 10.0f;
    const float labelStrip = 13.0f;
    const float titleStrip = 15.0f;  // holds the ER and tail labels, which track their own features
    auto plot = bounds.withTrimmedLeft(dbGutter)
                      .withTrimmedRight(rightPad)
                      .withTrimmedTop(titleStrip)
                      .withTrimmedBottom(labelStrip);

    if (plot.getWidth() <= 0.0f || plot.getHeight() <= 0.0f)
        return;

    auto timeToX = [&plot](float ms) {
        return plot.getX() + juce::jlimit(0.0f, 1.0f, ms / kMaxTimeMs) * plot.getWidth();
    };
    auto dbToY = [&plot](float db) {
        return plot.getBottom() - juce::jlimit(0.0f, 1.0f, (db - kMinDb) / -kMinDb) * plot.getHeight();
    };

    // Tail region, furthest back: where the reverb tail lives, decaying from its own level knob.
    // Shaded down to the floor with a brighter edge along the decay itself. Sampled per pixel column
    // so the arithmetic mode's curve comes out right; the exponential mode is a straight line.
    if (m_tailStartMs < kMaxTimeMs && m_tailLevelDb > kMinDb) {
        const float xStart = timeToX(m_tailStartMs);
        float xEnd = xStart;

        juce::Path edge;
        edge.startNewSubPath(xStart, dbToY(m_tailLevelDb));

        for (float x = xStart + 1.0f; x <= plot.getRight(); x += 1.0f) {
            const float ms = kMaxTimeMs * (x - plot.getX()) / plot.getWidth();
            const float db = tailLevelDbAt(ms);
            edge.lineTo(x, dbToY(db));   // dbToY clamps to the floor
            xEnd = x;

            if (db <= kMinDb)
                break;  // the tail has died; nothing left to draw
        }

        juce::Path body = edge;
        body.lineTo(xEnd, plot.getBottom());
        body.lineTo(xStart, plot.getBottom());
        body.closeSubPath();

        g.setColour(MyColours::accentColourAlternate.withAlpha(0.12f));
        g.fillPath(body);
        g.setColour(MyColours::accentColourAlternate.withAlpha(0.5f));
        g.strokePath(edge, juce::PathStrokeType(1.0f));
    }

    // The right channel follows the envelope-gram's colour map; the left stays white in both modes.
    const juce::Colour rightColour = m_envelopeGram.isColourMode() ? MyColours::erTapRightColour
                                                                   : MyColours::accentColourAlternate;

    // Pre-delay and tail-delay markers, behind everything else. The first tap is pinned to the
    // pre-delay, so that marker reads as a highlight band under the loudest tap.
    g.setColour(MyColours::erFillColour);
    for (float ms : { m_preDelayMs, m_tailDelayMs })
        g.fillRect(timeToX(ms) - 1.5f, plot.getY(), 3.0f, plot.getHeight());

    // Both axes are labels only: no grid, and the two markers are the only lines across the plot.
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    g.setColour(MyColours::accentColour.withAlpha(0.35f));

    // Time ticks
    for (int ms = 0; ms <= (int)kMaxTimeMs; ms += 100) {
        const float x = timeToX((float)ms);

        if (ms == (int)kMaxTimeMs) {
            // centring the last tick would push it past the right edge
            g.drawText(juce::String(ms) + " ms",
                       juce::Rectangle<float>(x - 44.0f, plot.getBottom(), 44.0f + rightPad - 2.0f, labelStrip),
                       juce::Justification::centredRight);
        }
        else {
            g.drawText(juce::String(ms),
                       juce::Rectangle<float>(x - 20.0f, plot.getBottom(), 40.0f, labelStrip),
                       juce::Justification::centred);
        }
    }

    // Level ticks
    for (int db = 0; db >= (int)kMinDb; db -= 20) {
        const float y = dbToY((float)db);

        // the bottom label sits above its row instead of centred on it, to stay clear of the 0 ms tick
        const float labelY = (db == (int)kMinDb) ? y - 12.0f : y - 6.0f;

        g.drawText(db == 0 ? "0 dB" : juce::String(db),
                   juce::Rectangle<float>(0.0f, labelY, dbGutter - 3.0f, 12.0f),
                   juce::Justification::centredRight);
    }

    // Taps: a stem down to the baseline plus a dot at the level. Both channels share the tap delay,
    // so they sit at the same x and coincide exactly at zero width. Right is drawn first so the
    // brighter left channel stays legible on top.
    const float dotRadius = 1.5f;  // 3 px
    int numDrawn = 0;
    for (int ch = 1; ch >= 0; --ch) {
        const juce::Colour colour = (ch == 0) ? MyColours::accentColour : rightColour;

        for (int i = 0; i < m_numTaps; ++i) {
            const float gain = (ch == 0) ? m_taps[(size_t)i].gainL : m_taps[(size_t)i].gainR;
            const float db = juce::Decibels::gainToDecibels(gain, kMinDb - 1.0f);
            if (db <= kMinDb)
                continue;  // below the floor there is no room for a stem

            const float ms = (float)m_taps[(size_t)i].delaySamples / (float)m_tapSampleRate * 1000.0f;
            const float x = timeToX(ms);
            const float y = dbToY(db);

            g.setColour(colour.withAlpha(0.55f));
            g.drawVerticalLine(juce::roundToInt(x), y, plot.getBottom());

            g.setColour(colour);
            g.fillEllipse(x - dotRadius, y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            ++numDrawn;
        }
    }

    // An empty plot needs to say which kind of empty it is: no taps at all, or a first tap the ER
    // level has pushed under the floor.
    if (numDrawn == 0) {
        g.setColour(MyColours::accentColour.withAlpha(0.4f));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
        g.drawText(m_numTaps == 0 ? "echoes off" : "er below " + juce::String((int)kMinDb) + " dB",
                   plot, juce::Justification::centred);
    }

    // Border and label, matching the envelope-gram underneath
    g.setColour(MyColours::accentColour.withAlpha(0.5f));
    g.drawRect(bounds, 1.0f);

    // Header labels, each sitting over the feature it names: "ER" with its channel legend above the
    // pre-delay marker, "tail" above the start of the tail region. Keeping the legend next to "ER"
    // is what stops the gray R from reading as a label for the gray tail region.
    const juce::Font titleFont(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold);
    g.setFont(titleFont);

    auto textWidth = [&titleFont](const juce::String& text) {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(titleFont, text, 0.0f, 0.0f);
        return glyphs.getBoundingBox(0, -1, true).getWidth();
    };

    const float gap   = textWidth(" ");
    const float wEr   = textWidth("ER");
    const float wL    = textWidth("L");
    const float wR    = textWidth("R");
    const float wTail = textWidth("tail");
    const float erBlockW = wEr + gap * 2.0f + wL + gap + wR;

    const float rightLimit = bounds.getRight() - 4.0f;

    const auto titleRow = bounds.withHeight(titleStrip);
    auto slot = [&titleRow, titleStrip](float x, float w) {
        return juce::Rectangle<float>(x, titleRow.getY(), w + 1.0f, titleStrip);
    };

    float erX = timeToX(m_preDelayMs) - 1.5f;  // lines up with the marker's left edge
    erX = juce::jlimit(2.0f, juce::jmax(2.0f, rightLimit - erBlockW), erX);

    g.setColour(MyColours::accentColour.withAlpha(0.6f));
    g.drawText("ER", slot(erX, wEr), juce::Justification::centredLeft);
    g.setColour(MyColours::accentColour);
    g.drawText("L", slot(erX + wEr + gap * 2.0f, wL), juce::Justification::centredLeft);
    g.setColour(rightColour);
    g.drawText("R", slot(erX + wEr + gap * 2.0f + wL + gap, wR), juce::Justification::centredLeft);

    // The tail label yields to the ER block and to the right edge, so it disappears when the tail
    // delay is at its maximum or crowds the early reflections.
    const float tailX = timeToX(m_tailStartMs) - 1.5f;
    const bool tailLabelFits = m_tailStartMs < kMaxTimeMs
                               && m_tailLevelDb > kMinDb
                               && tailX >= erX + erBlockW + gap * 2.0f
                               && tailX + wTail <= rightLimit;
    if (tailLabelFits) {
        g.setColour(MyColours::accentColour.withAlpha(0.6f));
        g.drawText("tail", slot(tailX, wTail), juce::Justification::centredLeft);
    }
}
