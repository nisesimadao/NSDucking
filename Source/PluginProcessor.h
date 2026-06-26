#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>

#include "CurveLibrary.h"

class NSDuckingAudioProcessor final : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    NSDuckingAudioProcessor();
    ~NSDuckingAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    APVTS& getState() { return parameters; }
    const APVTS& getState() const { return parameters; }
    float getEnvelopePosition() const noexcept { return uiEnvelopePosition.load(); }
    float getMidiActivity() const noexcept { return uiMidiActivity.load(); }
    float getCustomCurvePoint (size_t index) const noexcept;
    float getCustomVolumeAt (float position) const noexcept;
    void setCustomCurvePoint (size_t index, float value);
    void setCustomCurvePointRange (size_t startIndex, size_t endIndex, float startValue, float endValue);
    void loadPresetIntoCustomCurve (nsducking::CurvePreset preset);

    static APVTS::ParameterLayout createParameterLayout();

private:
    int getLengthInSamples() const;
    double getLengthFractionOfBar() const;
    float getTargetGain (int lengthSamples) const;
    void triggerEnvelope (float velocity);
    void advanceEnvelope (int lengthSamples);

    APVTS parameters;
    double currentSampleRate = 44100.0;
    int envelopeSample = -1;
    float smoothedGain = 1.0f;
    std::array<std::atomic<float>, nsducking::editablePointCount> customCurvePoints;

    std::atomic<float> uiEnvelopePosition { 1.0f };
    std::atomic<float> uiMidiActivity { 0.0f };
    std::atomic<float> lastVelocity { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NSDuckingAudioProcessor)
};
