// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

FDverbAudioProcessor::FDverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      m_fdverb(apvts)
{
    // Initialize display bins to zero
    for (auto& bin : envelopeDisplay) {
        bin.store(0.0f);
    }

    apvts.addParameterListener("FFTSIZE", this);
}

FDverbAudioProcessor::~FDverbAudioProcessor()
{
    apvts.removeParameterListener("FFTSIZE", this);
}

int FDverbAudioProcessor::fftSizeFromIndex(int index)
{
    static const int sizes[] = { 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };
    return sizes[juce::jlimit(0, 9, index)];
}

juce::AudioProcessorValueTreeState::ParameterLayout FDverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // custom gain range / mapping
    auto gainRange = juce::NormalisableRange<float>(
        -60.0f, // min
        0.0f,   // max
        [](float start, float end, float normalisedValue) {
            if (normalisedValue < 0.01f) {
                return -100.0f;
            }
            else if (normalisedValue < 0.50f) {
                float t = (normalisedValue - 0.01f) / 0.49f;
                return -60.0f + t * 50.0f;
            }
            else {
                float t = (normalisedValue - 0.50f) / 0.50f;
                return -10.0f + t * 10.0f;
            }
        },
        [](float start, float end, float value) {
            if (value <= -60.0f) {
                return 0.0f;
            }
            else if (value <= -10.0f) {
                float t = (value + 60.0f) / 50.0f;
                return t * 0.49f + 0.01f;
            }
            else {
                float t = (value + 10.0f) / 10.0f;
                return 0.5f + t * 0.5f;
            }
        }
    );

    // custom delay mapping
    auto makeDelayRange = [](float minMs, float maxMs, float steepness) {
        return juce::NormalisableRange<float>(
            minMs, maxMs,
            [minMs, maxMs, steepness](float start, float end, float norm) {
                return minMs * std::pow(maxMs / minMs, std::pow(norm, steepness));
            },
            [minMs, maxMs, steepness](float start, float end, float value) {
                return std::pow(std::log(value / minMs) / std::log(maxMs / minMs), 1.0f / steepness);
            }
        );
    };

    // custom frequency range (logarithmic)
    auto makeFreqRange = [](float minHz, float maxHz) {
        return juce::NormalisableRange<float>(
            minHz, maxHz,
            [minHz, maxHz](float start, float end, float norm) {
                return minHz * std::pow(maxHz / minHz, norm);
            },
            [minHz, maxHz](float start, float end, float value) {
                return std::log(value / minHz) / std::log(maxHz / minHz);
            }
        );
    };

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DRY", 1), "Dry", gainRange, -6.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TAIL", 1), "Tail", gainRange, -12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("PREDELAY", 1), "Predelay", makeDelayRange(1.0f, 500.0f, 0.5f), 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DECAY", 1), "Decay",
        makeDelayRange(100.0f, 10000.0f, 0.5f), 2000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TILT", 1), "Tilt",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("LOWCUT", 1), "Low Cut", makeFreqRange(20.0f, 20000.0f), 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("HIGHCUT", 1), "High Cut", makeFreqRange(20.0f, 20000.0f), 20000.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("LINK", 1), "Link", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("TAILDELAY", 1), "Tail Delay", makeDelayRange(1.0f, 500.0f, 0.5f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("WIDTH", 1), "Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("ERTAPS", 1), "ER Taps",
        0, 32, 16));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ERLEVEL", 1), "ER Level", gainRange, -12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("COLOR", 1), "Color",
        juce::NormalisableRange<float>(0.5f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("SHIFT", 1), "Shift",
        -100, 100, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("DRIFT", 1), "Drift",
        -100, 100, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("ENVMODE", 1), "Envelope Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("FFTSIZE", 1), "FFT Size",
        juce::StringArray{"32", "64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384"},
        8));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("FREEZE", 1), "Freeze", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("FREEZEINPUT", 1), "Freeze Input", false));

    return { params.begin(), params.end() };
}

// Preset table — order matches PRESET_FILES in python/yaml_to_preset.py.
// BinaryData names are generated by JUCE from the XML filenames:
// hyphens are stripped, other non-alphanumeric chars become underscores.
const FDverbAudioProcessor::PresetDescriptor FDverbAudioProcessor::kPresets[] = {
    { "Default",        BinaryData::default_xml,        BinaryData::default_xmlSize         },
    { "Dark Room",      BinaryData::darkroom_xml,       BinaryData::darkroom_xmlSize        },
    { "Live Room",      BinaryData::liveroom_xml,       BinaryData::liveroom_xmlSize        },
    { "Bell",           BinaryData::bell_xml,           BinaryData::bell_xmlSize            },
    { "Gated",          BinaryData::gated_xml,          BinaryData::gated_xmlSize           },
    { "Aura",           BinaryData::aura_xml,           BinaryData::aura_xmlSize            },
    { "Shifter",        BinaryData::shifter_xml,        BinaryData::shifter_xmlSize         },
    { "Drifter",        BinaryData::drifter_xml,        BinaryData::drifter_xmlSize         },
};
const int FDverbAudioProcessor::kNumPresets =
    (int)(sizeof(FDverbAudioProcessor::kPresets) / sizeof(FDverbAudioProcessor::kPresets[0]));

const juce::String FDverbAudioProcessor::getName() const { return JucePlugin_Name; }
bool FDverbAudioProcessor::acceptsMidi() const { return false; }
bool FDverbAudioProcessor::producesMidi() const { return false; }
bool FDverbAudioProcessor::isMidiEffect() const { return false; }
double FDverbAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int FDverbAudioProcessor::getNumPrograms() { return kNumPresets; }
int FDverbAudioProcessor::getCurrentProgram() { return m_currentProgram; }

void FDverbAudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= kNumPresets)
        return;

    m_currentProgram = index;
    const auto& preset = kPresets[index];
    auto xml = juce::parseXML(juce::String::fromUTF8(preset.xmlData, preset.xmlSize));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

const juce::String FDverbAudioProcessor::getProgramName(int index)
{
    if (index < 0 || index >= kNumPresets)
        return {};
    return kPresets[index].name;
}

void FDverbAudioProcessor::changeProgramName(int index, const juce::String& newName) {}

void FDverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    int fftSizeIndex = static_cast<int>(*apvts.getRawParameterValue("FFTSIZE"));
    int fftSize = fftSizeFromIndex(fftSizeIndex);

    m_fdverb.setFFTSizeAndReprepare(fftSize);  // safe even if not yet prepared — just stores blockSize
    m_fdverb.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
}

void FDverbAudioProcessor::releaseResources()
{
}

bool FDverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void FDverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // read parameters
    float dry = *apvts.getRawParameterValue("DRY");
    float tail = *apvts.getRawParameterValue("TAIL");
    float predelay = *apvts.getRawParameterValue("PREDELAY");
    float decay = *apvts.getRawParameterValue("DECAY");
    float tilt = *apvts.getRawParameterValue("TILT");
    float lowcut = *apvts.getRawParameterValue("LOWCUT");
    float highcut = *apvts.getRawParameterValue("HIGHCUT");
    bool link = *apvts.getRawParameterValue("LINK") > 0.5f;
    float taildelay = *apvts.getRawParameterValue("TAILDELAY");
    float width = *apvts.getRawParameterValue("WIDTH");
    int ertaps = static_cast<int>(*apvts.getRawParameterValue("ERTAPS"));
    float erlevel = *apvts.getRawParameterValue("ERLEVEL");
    float color = *apvts.getRawParameterValue("COLOR");
    int shift = static_cast<int>(*apvts.getRawParameterValue("SHIFT"));
    int drift = static_cast<int>(*apvts.getRawParameterValue("DRIFT"));
    bool envelopeModeIndex = static_cast<bool>(*apvts.getRawParameterValue("ENVMODE"));
    bool freeze = *apvts.getRawParameterValue("FREEZE") > 0.5f;
    bool freezeInput = *apvts.getRawParameterValue("FREEZEINPUT") > 0.5f;

    // pass to Fdverb
    m_fdverb.setDryLevelDb(dry);
    m_fdverb.setTailLevelDb(tail);
    m_fdverb.setPredelayMs(predelay);
    m_fdverb.setDecayMs(decay);
    m_fdverb.setTilt(tilt);
    m_fdverb.setLowCut(lowcut);
    m_fdverb.setHighCut(highcut);
    m_fdverb.setLinkEnvelopes(link);
    m_fdverb.setTaildelayMs(taildelay);
    m_fdverb.setWidth(width);
    m_fdverb.setNumERTaps(ertaps);
    m_fdverb.setERLevelDb(erlevel);
    m_fdverb.setColor(color);
    m_fdverb.setShift(shift);
    m_fdverb.setDrift(drift);

    std::string envelopeModes[] = {"exponential", "arithmetic"};
    m_fdverb.setEnvelopeMode(envelopeModes[envelopeModeIndex]);
    m_fdverb.setFreeze(freeze); // if freeze activated the envelope mode is ignored

    m_fdverb.setFreezeInput(freeze && freezeInput);

    m_fdverb.processBlock(buffer);

    updateEnvelopeDisplay();
}

bool FDverbAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* FDverbAudioProcessor::createEditor()
{
    return new FDverbAudioProcessorEditor(*this);
}

void FDverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("version", 1, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FDverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    // Parameters absent from the XML retain their current APVTS values (JUCE default).
    // Unknown properties (e.g. "version") are stored on the ValueTree and ignored by APVTS.
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FDverbAudioProcessor();
}

void FDverbAudioProcessor::updateEnvelopeDisplay()
{
    int numCh = getTotalNumInputChannels();
    const auto& envelope_l = m_fdverb.getEnvelope(0);
    const float* envelope_r_ptr = (numCh >= 2) ? m_fdverb.getEnvelope(1) : nullptr;

    size_t envelopeSize = m_fdverb.getEnvelopeSize();
    for (size_t i = 0; i < kNumDisplayBins; ++i) {
        size_t startBin = i * envelopeSize / kNumDisplayBins;
        size_t endBin   = (i + 1) * envelopeSize / kNumDisplayBins;
        // When envelopeSize < kNumDisplayBins, integer division makes startBin == endBin
        // for most bins, leaving them zero. Clamp to at least 1 bin (nearest-neighbour).
        if (endBin <= startBin)
            endBin = startBin + 1;
        endBin = std::min(endBin, envelopeSize);

        float maxEnv = 0;
        for (size_t j = startBin; j < endBin; ++j) {
            if (envelope_l[j] > maxEnv) maxEnv = envelope_l[j];
            if (envelope_r_ptr && envelope_r_ptr[j] > maxEnv) maxEnv = envelope_r_ptr[j];
        }
        envelopeDisplay[i].store(maxEnv);
    }
}

// debug function
int FDverbAudioProcessor::getCurrentFftSize() const
{
    return m_fdverb.getCurrentFftSize();
}

float FDverbAudioProcessor::getMinTailDelayMs() const
{
    return m_fdverb.getMinTailDelayMs();
}

void FDverbAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "FFTSIZE") {
        int newSize = fftSizeFromIndex(static_cast<int>(newValue));
        m_pendingFftSize.store(newSize);
        triggerAsyncUpdate();
    }
}

void FDverbAudioProcessor::handleAsyncUpdate()
{
    int newSize = m_pendingFftSize.exchange(-1);
    if (newSize < 0) {
        return;  // nothing pending (shouldn't happen)
    }

    if (newSize == m_fdverb.getCurrentFftSize()) {
        return;  // no actual change
    }

    suspendProcessing(true);
    m_fdverb.setFFTSizeAndReprepare(newSize);
    suspendProcessing(false);
}
