#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
constexpr auto depthId          = "depth";
constexpr auto lengthId         = "length";
constexpr auto smoothId         = "smooth";
constexpr auto presetId         = "preset";
constexpr auto outputGainId     = "outputGain";
constexpr auto tabId            = "uiTab";
constexpr auto triggerModeId    = "triggerMode";
constexpr auto velocitySensId   = "velocitySens";
constexpr auto retriggerModeId  = "retriggerMode";
constexpr auto noteFilterOnId   = "noteFilterOn";
constexpr auto noteFilterNoteId = "noteFilterNote";
constexpr auto customCurveProperty = "customCurve";

constexpr std::array<float, 7> lengthFractionsOfBar {
    1.0f / 64.0f,
    1.0f / 32.0f,
    1.0f / 16.0f,
    1.0f / 8.0f,
    1.0f / 4.0f,
    1.0f / 2.0f,
    1.0f
};

juce::StringArray getLengthNames()
{
    return { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar" };
}

juce::StringArray getPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : nsducking::presetInfos)
        names.add (juce::String (preset.name.data()));
    return names;
}

juce::StringArray getNoteNames()
{
    static constexpr const char* names12[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::StringArray names;
    for (int i = 0; i < 128; ++i)
        names.add (juce::String (names12[i % 12]) + juce::String (i / 12 - 2) + " (" + juce::String (i) + ")");
    return names;
}

template <typename T>
T getChoiceIndex (const juce::AudioProcessorValueTreeState& parameters, const juce::String& id, T fallback)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter (id)))
        return static_cast<T> (parameter->getIndex());

    return fallback;
}
} // namespace

NSDuckingAudioProcessor::NSDuckingAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    loadPresetIntoCustomCurve (nsducking::CurvePreset::classic);
}

void NSDuckingAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    envelopeSample = -1;
    smoothedGain = 1.0f;
    uiEnvelopePosition.store (1.0f);
    uiMidiActivity.store (0.0f);
}

void NSDuckingAudioProcessor::releaseResources()
{
}

bool NSDuckingAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    return input == output && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
}

void NSDuckingAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const auto smoothMs         = parameters.getRawParameterValue (smoothId)->load();
    const auto smoothSamples    = static_cast<float> (juce::jmax (1.0, currentSampleRate * smoothMs * 0.001));
    const auto smoothCoefficient = smoothSamples <= 1.0f ? 0.0f : std::exp (-1.0f / smoothSamples);
    const auto outputGain       = juce::Decibels::decibelsToGain (parameters.getRawParameterValue (outputGainId)->load());
    const auto lengthSamples    = getLengthInSamples();

    const auto processSampleRange = [&] (int startSample, int endSample)
    {
        for (auto sample = startSample; sample < endSample; ++sample)
        {
            const auto targetGain = getTargetGain (lengthSamples);
            smoothedGain = smoothCoefficient <= 0.0f
                               ? targetGain
                               : targetGain + smoothCoefficient * (smoothedGain - targetGain);

            const auto finalGain = smoothedGain * outputGain;
            for (auto channel = 0; channel < totalNumInputChannels; ++channel)
                buffer.setSample (channel, sample, buffer.getSample (channel, sample) * finalGain);

            advanceEnvelope (lengthSamples);
        }
    };

    const auto triggerMode     = getChoiceIndex<int> (parameters, triggerModeId, 0);
    const auto retriggerAlways = getChoiceIndex<int> (parameters, retriggerModeId, 0) == 0;
    const auto noteFilterOn    = *parameters.getRawParameterValue (noteFilterOnId) > 0.5f;
    const auto filterNote      = getChoiceIndex<int> (parameters, noteFilterNoteId, 36);

    auto currentSample = 0;

    if (triggerMode == 0) // MIDI trigger
    {
        for (const auto metadata : midiMessages)
        {
            const auto eventSample = juce::jlimit (0, buffer.getNumSamples(), metadata.samplePosition);
            processSampleRange (currentSample, eventSample);

            if (metadata.getMessage().isNoteOn())
            {
                const auto noteNumber = metadata.getMessage().getNoteNumber();
                if (! noteFilterOn || noteNumber == filterNote)
                {
                    if (retriggerAlways || envelopeSample < 0)
                    {
                        triggerEnvelope (metadata.getMessage().getVelocity() / 127.0f);
                    }
                }
            }

            currentSample = eventSample;
        }
    }
    else if (triggerMode == 1) // Host Sync
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                const bool isPlaying = position->getIsPlaying();
                if (isPlaying && position->getBpm() && position->getPpqPosition())
                {
                    const double ppq        = *position->getPpqPosition();
                    const double bpm        = *position->getBpm();
                    const double periodBeats = 4.0 * getLengthFractionOfBar();
                    const double blockBeats  = buffer.getNumSamples() / currentSampleRate * bpm / 60.0;
                    const double ppqEnd      = ppq + blockBeats;

                    // Iterate over every sync boundary that falls within this block
                    const double firstBoundary = std::ceil (ppq / periodBeats) * periodBeats;
                    for (double boundary = firstBoundary; boundary < ppqEnd - 1e-9; boundary += periodBeats)
                    {
                        const double frac      = (boundary - ppq) / blockBeats;
                        const int    trigSample = juce::jlimit (0, buffer.getNumSamples() - 1,
                                                               static_cast<int> (std::round (frac * buffer.getNumSamples())));
                        processSampleRange (currentSample, trigSample);
                        triggerEnvelope (1.0f);
                        currentSample = trigSample;
                    }
                }
            }
        }
    }

    processSampleRange (currentSample, buffer.getNumSamples());

    uiMidiActivity.store (juce::jmax (0.0f, uiMidiActivity.load() - 0.035f));
}

juce::AudioProcessorEditor* NSDuckingAudioProcessor::createEditor()
{
    return new NSDuckingAudioProcessorEditor (*this);
}

void NSDuckingAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    juce::StringArray pointValues;
    for (size_t i = 0; i < customCurvePoints.size(); ++i)
        pointValues.add (juce::String (getCustomCurvePoint (i), 5));

    state.setProperty (customCurveProperty, pointValues.joinIntoString (";"), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NSDuckingAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xml));

            if (auto encodedCurve = parameters.state.getProperty (customCurveProperty).toString(); encodedCurve.isNotEmpty())
            {
                const auto pointValues = juce::StringArray::fromTokens (encodedCurve, ";", "");
                for (auto i = 0; i < juce::jmin (static_cast<int> (customCurvePoints.size()), pointValues.size()); ++i)
                    customCurvePoints[static_cast<size_t> (i)].store (juce::jlimit (0.0f, 1.0f, pointValues[i].getFloatValue()));
            }
        }
}

NSDuckingAudioProcessor::APVTS::ParameterLayout NSDuckingAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { depthId, 1 }, "Depth", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.72f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
            [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)); })));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { lengthId, 1 }, "Length", getLengthNames(), 4));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { smoothId, 1 }, "Smooth", juce::NormalisableRange<float> { 0.0f, 20.0f, 0.01f }, 2.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { presetId, 1 }, "Curve", getPresetNames(), 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGainId, 1 }, "Output Gain", juce::NormalisableRange<float> { -12.0f, 12.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { triggerModeId, 1 }, "Trigger Mode", juce::StringArray { "MIDI", "Host Sync" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { tabId, 1 }, "UI Tab", juce::StringArray { "Main", "MIDI", "About" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { velocitySensId, 1 }, "Velocity Sensitivity",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)); })));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { retriggerModeId, 1 }, "Retrigger",
        juce::StringArray { "Always", "Free Run" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { noteFilterOnId, 1 }, "Note Filter", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { noteFilterNoteId, 1 }, "Filter Note", getNoteNames(), 36));

    return { params.begin(), params.end() };
}

double NSDuckingAudioProcessor::getLengthFractionOfBar() const
{
    const auto index = juce::jlimit (0, static_cast<int> (lengthFractionsOfBar.size()) - 1,
                                     getChoiceIndex<int> (parameters, lengthId, 4));
    return static_cast<double> (lengthFractionsOfBar[static_cast<size_t> (index)]);
}

int NSDuckingAudioProcessor::getLengthInSamples() const
{
    auto bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto hostBpm = position->getBpm(); hostBpm && *hostBpm > 1.0)
                bpm = *hostBpm;

    const auto beats  = 4.0 * getLengthFractionOfBar();
    const auto seconds = beats * 60.0 / bpm;
    return juce::jmax (1, static_cast<int> (std::round (seconds * currentSampleRate)));
}

float NSDuckingAudioProcessor::getTargetGain (int lengthSamples) const
{
    if (envelopeSample < 0)
        return 1.0f;

    const auto position    = static_cast<float> (envelopeSample) / static_cast<float> (lengthSamples);
    const auto curveVolume = getCustomVolumeAt (position);
    const auto depth       = parameters.getRawParameterValue (depthId)->load();
    const auto velocitySens = parameters.getRawParameterValue (velocitySensId)->load();
    const auto velocity    = lastVelocity.load();

    // blend between full depth and velocity-scaled depth
    const auto effectiveDepth = depth * (1.0f + velocitySens * (velocity - 1.0f));

    return juce::jlimit (0.0f, 1.0f, 1.0f - effectiveDepth * (1.0f - curveVolume));
}

float NSDuckingAudioProcessor::getCustomCurvePoint (size_t index) const noexcept
{
    if (index >= customCurvePoints.size())
        return 1.0f;

    return customCurvePoints[index].load();
}

float NSDuckingAudioProcessor::getCustomVolumeAt (float position) const noexcept
{
    const auto x      = juce::jlimit (0.0f, 1.0f, position);
    const auto scaled = x * static_cast<float> (customCurvePoints.size() - 1);
    const auto left   = static_cast<size_t> (juce::jlimit (0, static_cast<int> (customCurvePoints.size() - 1),
                                                            static_cast<int> (std::floor (scaled))));
    const auto right  = juce::jmin (left + 1, customCurvePoints.size() - 1);
    const auto amount = scaled - static_cast<float> (left);
    return juce::jlimit (0.0f, 1.0f,
                         getCustomCurvePoint (left) + (getCustomCurvePoint (right) - getCustomCurvePoint (left)) * amount);
}

void NSDuckingAudioProcessor::setCustomCurvePoint (size_t index, float value)
{
    if (index >= customCurvePoints.size())
        return;

    customCurvePoints[index].store (juce::jlimit (0.0f, 1.0f, value));
}

void NSDuckingAudioProcessor::setCustomCurvePointRange (size_t startIndex, size_t endIndex,
                                                         float startValue, float endValue)
{
    if (startIndex > endIndex)
    {
        std::swap (startIndex, endIndex);
        std::swap (startValue, endValue);
    }

    startIndex = juce::jmin (startIndex, customCurvePoints.size() - 1);
    endIndex   = juce::jmin (endIndex,   customCurvePoints.size() - 1);

    const auto width = juce::jmax (1.0f, static_cast<float> (endIndex - startIndex));
    for (auto index = startIndex; index <= endIndex; ++index)
    {
        const auto amount = static_cast<float> (index - startIndex) / width;
        setCustomCurvePoint (index, startValue + (endValue - startValue) * amount);
    }
}

void NSDuckingAudioProcessor::loadPresetIntoCustomCurve (nsducking::CurvePreset preset)
{
    const auto points = nsducking::makeVolumePoints (preset);
    for (size_t i = 0; i < customCurvePoints.size(); ++i)
        customCurvePoints[i].store (points[i]);
}

void NSDuckingAudioProcessor::triggerEnvelope (float velocity)
{
    lastVelocity.store (juce::jlimit (0.0f, 1.0f, velocity));
    envelopeSample = 0;
    uiEnvelopePosition.store (0.0f);
    uiMidiActivity.store (1.0f);
}

void NSDuckingAudioProcessor::advanceEnvelope (int lengthSamples)
{
    if (envelopeSample < 0)
        return;

    ++envelopeSample;

    if (envelopeSample >= lengthSamples)
    {
        envelopeSample = -1;
        uiEnvelopePosition.store (1.0f);
        return;
    }

    uiEnvelopePosition.store (static_cast<float> (envelopeSample) / static_cast<float> (lengthSamples));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NSDuckingAudioProcessor();
}
