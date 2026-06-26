#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <optional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"

class NSDuckingLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NSDuckingLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class CurveDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit CurveDisplay (NSDuckingAudioProcessor&);
    ~CurveDisplay() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    nsducking::CurvePreset getSelectedPreset() const;
    juce::Rectangle<float> getPlotBounds() const;
    size_t eventToHandleIndex (const juce::MouseEvent&) const;
    float eventToHandleValue (const juce::MouseEvent&) const;
    void refreshHandlesFromProcessor();
    void writeHandlesToProcessor();
    void applyMouseEdit (const juce::MouseEvent&);

    NSDuckingAudioProcessor& processor;
    std::array<float, 6> handles {};
    std::optional<size_t> activeHandle;
};

class PresetButton final : public juce::Button
{
public:
    explicit PresetButton (nsducking::PresetInfo presetInfo);
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    nsducking::PresetInfo preset;
};

class NSDuckingAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NSDuckingAudioProcessorEditor (NSDuckingAudioProcessor&);
    ~NSDuckingAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void configureRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& text, bool large);
    void configureHorizontalSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void configureTabButton (juce::TextButton& button, int tabIndex);
    void setCurrentTab (int tabIndex);
    void updateTabButtons();
    void updatePresetButtons();
    void timerCallback() override;

    NSDuckingAudioProcessor& audioProcessor;
    NSDuckingLookAndFeel lookAndFeel;

    // Header
    juce::Label    logoLabel;
    juce::TextButton mainTab  { "Main" };
    juce::TextButton midiTab  { "MIDI" };
    juce::TextButton aboutTab { "About" };
    juce::Component midiIndicator;

    // Main page
    juce::Slider   depthSlider;
    juce::Slider   smoothSlider;
    juce::Slider   outputSlider;
    juce::ComboBox lengthBox;
    juce::Label    depthLabel;
    juce::Label    smoothLabel;
    juce::Label    outputLabel;
    juce::Label    lengthLabel;
    CurveDisplay   curveDisplay;
    std::vector<std::unique_ptr<PresetButton>> presetButtons;

    // MIDI page
    juce::ComboBox triggerModeCombo;
    juce::Label    triggerModeLabel;
    juce::Slider   velocitySensSlider;
    juce::Label    velocitySensLabel;
    juce::ComboBox retriggerCombo;
    juce::Label    retriggerLabel;
    juce::ToggleButton noteFilterButton;
    juce::ComboBox noteFilterCombo;
    juce::Label    noteFilterLabel;

    // About page
    juce::Label    aboutPageLabel;

    int currentTab = 0;

    // Main page attachments
    std::unique_ptr<SliderAttachment>   depthAttachment;
    std::unique_ptr<SliderAttachment>   smoothAttachment;
    std::unique_ptr<SliderAttachment>   outputAttachment;
    std::unique_ptr<ComboBoxAttachment> lengthAttachment;

    // MIDI page attachments
    std::unique_ptr<ComboBoxAttachment> triggerModeAttachment;
    std::unique_ptr<SliderAttachment>   velocitySensAttachment;
    std::unique_ptr<ComboBoxAttachment> retriggerAttachment;
    std::unique_ptr<ButtonAttachment>   noteFilterAttachment;
    std::unique_ptr<ComboBoxAttachment> noteFilterNoteAttachment;
};
