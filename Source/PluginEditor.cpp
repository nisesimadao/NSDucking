#include "PluginEditor.h"

#include <cmath>
#include <functional>

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

const juce::Colour background { 0xff15171a };
const juce::Colour panel      { 0xff24282d };
const juce::Colour panelLight { 0xff30363d };
const juce::Colour text       { 0xfff2f5f8 };
const juce::Colour mutedText  { 0xff9aa4ad };
const juce::Colour accent     { 0xff38c7f3 };
const juce::Colour hotAccent  { 0xffff715b };

template <typename T>
T* choiceParam (NSDuckingAudioProcessor::APVTS& parameters, const juce::String& id)
{
    return dynamic_cast<T*> (parameters.getParameter (id));
}

juce::Font labelFont()
{
    return juce::Font (juce::FontOptions (12.0f));
}

juce::Font titleFont()
{
    return juce::Font (juce::FontOptions (22.0f).withStyle ("Bold"));
}

juce::Font sectionFont()
{
    return juce::Font (juce::FontOptions (11.0f).withStyle ("Bold"));
}

float smoothHandleValue (const std::array<float, 6>& handles, float position)
{
    const auto x      = juce::jlimit (0.0f, 1.0f, position);
    const auto scaled = x * static_cast<float> (handles.size() - 1);
    const auto i1     = juce::jlimit (0, static_cast<int> (handles.size() - 1), static_cast<int> (std::floor (scaled)));
    const auto i0     = juce::jmax (0, i1 - 1);
    const auto i2     = juce::jmin (static_cast<int> (handles.size() - 1), i1 + 1);
    const auto i3     = juce::jmin (static_cast<int> (handles.size() - 1), i1 + 2);
    const auto t      = scaled - static_cast<float> (i1);
    const auto t2     = t * t;
    const auto t3     = t2 * t;

    const auto p0    = handles[static_cast<size_t> (i0)];
    const auto p1    = handles[static_cast<size_t> (i1)];
    const auto p2    = handles[static_cast<size_t> (i2)];
    const auto p3    = handles[static_cast<size_t> (i3)];
    const auto value = 0.5f * ((2.0f * p1) + (-p0 + p2) * t
                               + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                               + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

    return juce::jlimit (0.0f, 1.0f, value);
}

void drawVolumePath (juce::Graphics& g, juce::Rectangle<float> plot,
                     const std::function<float (float)>& valueAt,
                     juce::Colour lineColour, juce::Colour fillColour, float strokeWidth)
{
    juce::Path curve;
    juce::Path fill;
    const auto steps = juce::jmax (24, juce::roundToInt (plot.getWidth()));

    for (auto i = 0; i <= steps; ++i)
    {
        const auto xNorm = static_cast<float> (i) / static_cast<float> (steps);
        const auto value = valueAt (xNorm);
        const auto x     = plot.getX() + xNorm * plot.getWidth();
        const auto y     = plot.getBottom() - value * plot.getHeight();

        if (i == 0)
        {
            curve.startNewSubPath (x, y);
            fill.startNewSubPath (x, plot.getBottom());
            fill.lineTo (x, y);
        }
        else
        {
            curve.lineTo (x, y);
            fill.lineTo (x, y);
        }
    }

    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();

    g.setColour (fillColour);
    g.fillPath (fill);
    g.setColour (lineColour.withAlpha (0.35f));
    g.strokePath (curve, juce::PathStrokeType (strokeWidth + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (lineColour);
    g.strokePath (curve, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

// ── LookAndFeel ─────────────────────────────────────────────────────────────

NSDuckingLookAndFeel::NSDuckingLookAndFeel()
{
    setColour (juce::Slider::thumbColourId,               accent);
    setColour (juce::Slider::rotarySliderFillColourId,    accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour { 0xff111315u });
    setColour (juce::Slider::trackColourId,               accent);
    setColour (juce::Slider::backgroundColourId,          panelLight);
    setColour (juce::TextButton::buttonColourId,          panelLight);
    setColour (juce::TextButton::buttonOnColourId,        accent);
    setColour (juce::TextButton::textColourOffId,         text);
    setColour (juce::TextButton::textColourOnId,          juce::Colour { 0xff071014u });
    setColour (juce::ComboBox::backgroundColourId,        panelLight);
    setColour (juce::ComboBox::textColourId,              text);
    setColour (juce::ComboBox::outlineColourId,           juce::Colour { 0xff48505au });
    setColour (juce::PopupMenu::backgroundColourId,       panel);
    setColour (juce::PopupMenu::textColourId,             text);
    setColour (juce::ToggleButton::textColourId,          text);
    setColour (juce::ToggleButton::tickColourId,          accent);
    setColour (juce::ToggleButton::tickDisabledColourId,  mutedText);
}

void NSDuckingLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider&)
{
    const auto rawBounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                   static_cast<float> (width), static_cast<float> (height))
                               .reduced (4.0f);
    const auto diameter = juce::jmin (rawBounds.getWidth(), rawBounds.getHeight());
    const auto bounds   = juce::Rectangle<float> (diameter, diameter).withCentre (rawBounds.getCentre());
    const auto radius   = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre   = bounds.getCentre();
    const auto angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setGradientFill (juce::ColourGradient (juce::Colour { 0xff3d444du }, centre.x, bounds.getY(),
                                             juce::Colour { 0xff171a1du }, centre.x, bounds.getBottom(), false));
    g.fillEllipse (bounds);
    g.setColour (juce::Colour { 0xff090a0cu });
    g.drawEllipse (bounds, 1.5f);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 5.0f, radius - 5.0f, 0.0f,
                       rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto markerLength = radius * 0.55f;
    const auto markerStart  = centre.getPointOnCircumference (radius * 0.22f, angle);
    const auto markerEnd    = centre.getPointOnCircumference (markerLength, angle);
    g.setColour (text);
    g.drawLine ({ markerStart, markerEnd }, 2.4f);
}

void NSDuckingLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                             juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    const float trackY      = y + height * 0.5f;
    const float trackHeight = 4.0f;
    const auto  trackRect   = juce::Rectangle<float> (static_cast<float> (x), trackY - trackHeight * 0.5f,
                                                       static_cast<float> (width), trackHeight);

    g.setColour (panelLight.brighter (0.1f));
    g.fillRoundedRectangle (trackRect, 2.0f);

    const auto filled = trackRect.withRight (sliderPos);
    g.setColour (accent);
    g.fillRoundedRectangle (filled, 2.0f);

    const float thumbR = 7.0f;
    g.setColour (accent);
    g.fillEllipse (sliderPos - thumbR, trackY - thumbR, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour (juce::Colour { 0xff090a0cu });
    g.drawEllipse (sliderPos - thumbR, trackY - thumbR, thumbR * 2.0f, thumbR * 2.0f, 1.0f);
}

void NSDuckingLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds  = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto on      = button.getToggleState();
    const auto base    = on ? accent.withAlpha (0.22f) : (shouldDrawButtonAsHighlighted ? panelLight.brighter (0.1f) : panelLight);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (on ? accent : juce::Colour { 0xff46505au });
    g.drawRoundedRectangle (bounds, 4.0f, on ? 1.8f : 1.0f);

    // Tick indicator
    const float tickSize = juce::jmin (bounds.getHeight() * 0.45f, 10.0f);
    const float tickX    = bounds.getX() + 8.0f;
    const float tickY    = bounds.getCentreY() - tickSize * 0.5f;
    g.setColour (on ? accent : mutedText);
    g.fillEllipse (tickX, tickY, tickSize, tickSize);

    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.setColour (on ? text : mutedText);
    auto textArea = bounds.toNearestInt();
    textArea.removeFromLeft (static_cast<int> (tickX - bounds.getX()) + static_cast<int> (tickSize) + 6);
    g.drawText (button.getButtonText(), textArea, juce::Justification::centredLeft, true);
}

// ── CurveDisplay ─────────────────────────────────────────────────────────────

CurveDisplay::CurveDisplay (NSDuckingAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    refreshHandlesFromProcessor();
    startTimerHz (30);
}

CurveDisplay::~CurveDisplay()
{
    stopTimer();
}

void CurveDisplay::paint (juce::Graphics& g)
{
    const auto bounds      = getLocalBounds().toFloat().reduced (2.0f);
    const auto plot        = getPlotBounds();
    const auto gridColumns = getWidth() > 560 ? 16 : (getWidth() > 360 ? 8 : 4);

    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colour { 0xff3a4149u });
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setColour (juce::Colour { 0x224f5962u });
    for (auto i = 0; i <= gridColumns; ++i)
    {
        const auto x = plot.getX() + plot.getWidth() * static_cast<float> (i) / static_cast<float> (gridColumns);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }
    for (auto i = 0; i <= 4; ++i)
    {
        const auto y = plot.getY() + plot.getHeight() * static_cast<float> (i) / 4.0f;
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }

    const auto preset = getSelectedPreset();
    drawVolumePath (g, plot, [this] (float x) { return processor.getCustomVolumeAt (x); },
                    accent, accent.withAlpha (0.18f), 2.4f);

    const auto playhead  = processor.getEnvelopePosition();
    const auto playheadX = plot.getX() + playhead * plot.getWidth();
    g.setColour (hotAccent.withAlpha (processor.getMidiActivity() > 0.0f ? 0.95f : 0.35f));
    g.drawLine ({ playheadX, plot.getY(), playheadX, plot.getBottom() }, 2.0f);

    g.setFont (labelFont());
    g.setColour (mutedText);
    g.drawText ("Volume Curve", bounds.toNearestInt().reduced (16, 10), juce::Justification::topLeft);
    g.drawText (juce::String (nsducking::getPresetName (preset).data()), bounds.toNearestInt().reduced (16, 10),
                juce::Justification::topRight);

    for (size_t i = 0; i < handles.size(); ++i)
    {
        const auto xNorm  = static_cast<float> (i) / static_cast<float> (handles.size() - 1);
        const auto value  = handles[i];
        const auto point  = juce::Point<float> (plot.getX() + xNorm * plot.getWidth(),
                                                plot.getBottom() - value * plot.getHeight());
        const bool active = (i == activeHandle.value_or (handles.size()));
        g.setColour (active ? hotAccent : text.withAlpha (0.86f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (point));
        g.setColour (background.withAlpha (0.82f));
        g.drawEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (point), 1.0f);
    }
}

void CurveDisplay::mouseDown (const juce::MouseEvent& event)
{
    refreshHandlesFromProcessor();
    activeHandle = eventToHandleIndex (event);
    applyMouseEdit (event);
}

void CurveDisplay::mouseDrag (const juce::MouseEvent& event)
{
    applyMouseEdit (event);
}

void CurveDisplay::mouseUp (const juce::MouseEvent&)
{
    activeHandle.reset();
}

void CurveDisplay::timerCallback()
{
    if (! activeHandle)
        refreshHandlesFromProcessor();

    repaint();
}

nsducking::CurvePreset CurveDisplay::getSelectedPreset() const
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (processor.getState().getParameter (presetId)))
        return nsducking::presetFromIndex (parameter->getIndex());

    return nsducking::CurvePreset::classic;
}

juce::Rectangle<float> CurveDisplay::getPlotBounds() const
{
    return getLocalBounds().toFloat().reduced (18.0f, 18.0f).withTrimmedTop (8.0f);
}

size_t CurveDisplay::eventToHandleIndex (const juce::MouseEvent& event) const
{
    const auto plot  = getPlotBounds();
    const auto xNorm = juce::jlimit (0.0f, 1.0f,
                                     (event.position.x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
    const auto index = static_cast<size_t> (juce::roundToInt (xNorm * static_cast<float> (handles.size() - 1)));
    return juce::jmin (index, handles.size() - 1);
}

float CurveDisplay::eventToHandleValue (const juce::MouseEvent& event) const
{
    const auto plot = getPlotBounds();
    return juce::jlimit (0.0f, 1.0f, 1.0f - ((event.position.y - plot.getY()) / juce::jmax (1.0f, plot.getHeight())));
}

void CurveDisplay::refreshHandlesFromProcessor()
{
    for (size_t i = 0; i < handles.size(); ++i)
    {
        const auto xNorm = static_cast<float> (i) / static_cast<float> (handles.size() - 1);
        handles[i]       = processor.getCustomVolumeAt (xNorm);
    }
}

void CurveDisplay::writeHandlesToProcessor()
{
    for (size_t i = 0; i < nsducking::editablePointCount; ++i)
    {
        const auto xNorm = static_cast<float> (i) / static_cast<float> (nsducking::editablePointCount - 1);
        processor.setCustomCurvePoint (i, smoothHandleValue (handles, xNorm));
    }
}

void CurveDisplay::applyMouseEdit (const juce::MouseEvent& event)
{
    if (! activeHandle)
        activeHandle = eventToHandleIndex (event);

    handles[*activeHandle] = eventToHandleValue (event);
    writeHandlesToProcessor();
    repaint();
}

// ── PresetButton ──────────────────────────────────────────────────────────────

PresetButton::PresetButton (nsducking::PresetInfo presetInfo)
    : Button (juce::String (presetInfo.name.data())),
      preset (presetInfo)
{
    setClickingTogglesState (false);
}

void PresetButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds   = getLocalBounds().toFloat().reduced (1.0f);
    const auto selected = getToggleState();
    const auto base     = selected ? accent.withAlpha (0.22f)
                                   : (shouldDrawButtonAsHighlighted ? panelLight.brighter (0.08f) : panelLight);

    g.setColour (shouldDrawButtonAsDown ? base.darker (0.18f) : base);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (selected ? accent : juce::Colour { 0xff46505au });
    g.drawRoundedRectangle (bounds, 6.0f, selected ? 1.8f : 1.0f);

    auto content  = getLocalBounds().reduced (7, 5);
    auto nameArea = content.removeFromBottom (16);
    auto plot     = content.toFloat().reduced (1.0f, 2.0f);

    g.setColour (juce::Colour { 0x2638c7f3u });
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

    drawVolumePath (g, plot, [this] (float x) { return nsducking::evaluateVolumeCurve (preset.preset, x); },
                    selected ? accent : juce::Colour { 0xfff2f5f8u }.withAlpha (0.72f),
                    (selected ? accent : juce::Colour { 0xfff2f5f8u }).withAlpha (0.08f), 1.4f);

    g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    g.setColour (selected ? juce::Colour { 0xfff2f5f8u } : mutedText);
    g.drawText (juce::String (preset.name.data()), nameArea, juce::Justification::centred, true);
}

// ── Editor ────────────────────────────────────────────────────────────────────

NSDuckingAudioProcessorEditor::NSDuckingAudioProcessorEditor (NSDuckingAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse),
      audioProcessor (processorToUse),
      curveDisplay (processorToUse)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (520, 340, 980, 700);
    setSize (760, 480);

    logoLabel.setText ("NSDucking", juce::dontSendNotification);
    logoLabel.setFont (titleFont());
    logoLabel.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (logoLabel);

    configureTabButton (mainTab, 0);
    configureTabButton (midiTab, 1);
    configureTabButton (aboutTab, 2);

    midiIndicator.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (midiIndicator);

    // Main page controls
    configureRotarySlider (depthSlider, depthLabel, "Depth", true);
    configureRotarySlider (smoothSlider, smoothLabel, "Smooth", false);
    configureRotarySlider (outputSlider, outputLabel, "Output", false);

    lengthLabel.setText ("Length", juce::dontSendNotification);
    lengthLabel.setFont (labelFont());
    lengthLabel.setColour (juce::Label::textColourId, mutedText);
    addAndMakeVisible (lengthLabel);

    lengthBox.addItemList ({ "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar" }, 1);
    addAndMakeVisible (lengthBox);
    addAndMakeVisible (curveDisplay);

    for (const auto& preset : nsducking::presetInfos)
    {
        auto button = std::make_unique<PresetButton> (preset);
        button->onClick = [this, index = nsducking::presetToIndex (preset.preset)]
        {
            audioProcessor.loadPresetIntoCustomCurve (nsducking::presetFromIndex (index));

            if (auto* parameter = choiceParam<juce::AudioParameterChoice> (audioProcessor.getState(), presetId))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
                parameter->endChangeGesture();
            }
            updatePresetButtons();
        };
        addAndMakeVisible (*button);
        presetButtons.push_back (std::move (button));
    }

    // MIDI page controls
    triggerModeLabel.setText ("Trigger Mode", juce::dontSendNotification);
    triggerModeLabel.setFont (labelFont());
    triggerModeLabel.setColour (juce::Label::textColourId, mutedText);
    addChildComponent (triggerModeLabel);

    triggerModeCombo.addItemList ({ "MIDI", "Host Sync" }, 1);
    addChildComponent (triggerModeCombo);

    configureHorizontalSlider (velocitySensSlider, velocitySensLabel, "Velocity Sensitivity");

    retriggerLabel.setText ("Retrigger", juce::dontSendNotification);
    retriggerLabel.setFont (labelFont());
    retriggerLabel.setColour (juce::Label::textColourId, mutedText);
    addChildComponent (retriggerLabel);

    retriggerCombo.addItemList ({ "Always", "Free Run" }, 1);
    addChildComponent (retriggerCombo);

    noteFilterLabel.setText ("Note Filter", juce::dontSendNotification);
    noteFilterLabel.setFont (labelFont());
    noteFilterLabel.setColour (juce::Label::textColourId, mutedText);
    addChildComponent (noteFilterLabel);

    noteFilterButton.setButtonText ("Enable");
    addChildComponent (noteFilterButton);

    // Populate note filter combo with note names
    {
        static constexpr const char* names12[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 128; ++i)
            noteFilterCombo.addItem (juce::String (names12[i % 12]) + juce::String (i / 12 - 2)
                                         + " (" + juce::String (i) + ")",
                                     i + 1);
    }
    addChildComponent (noteFilterCombo);

    // About page
    aboutPageLabel.setFont (juce::Font (juce::FontOptions (15.0f)));
    aboutPageLabel.setColour (juce::Label::textColourId, text);
    aboutPageLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (aboutPageLabel);

    // Attach parameters
    auto& state = audioProcessor.getState();
    depthAttachment  = std::make_unique<SliderAttachment>   (state, depthId,       depthSlider);
    smoothAttachment = std::make_unique<SliderAttachment>   (state, smoothId,      smoothSlider);
    outputAttachment = std::make_unique<SliderAttachment>   (state, outputGainId,  outputSlider);
    lengthAttachment = std::make_unique<ComboBoxAttachment> (state, lengthId,      lengthBox);

    triggerModeAttachment   = std::make_unique<ComboBoxAttachment> (state, triggerModeId,    triggerModeCombo);
    velocitySensAttachment  = std::make_unique<SliderAttachment>   (state, velocitySensId,   velocitySensSlider);
    retriggerAttachment     = std::make_unique<ComboBoxAttachment> (state, retriggerModeId,  retriggerCombo);
    noteFilterAttachment    = std::make_unique<ButtonAttachment>   (state, noteFilterOnId,   noteFilterButton);
    noteFilterNoteAttachment = std::make_unique<ComboBoxAttachment> (state, noteFilterNoteId, noteFilterCombo);

    if (auto* parameter = choiceParam<juce::AudioParameterChoice> (state, tabId))
        currentTab = parameter->getIndex();

    setCurrentTab (currentTab);
    startTimerHz (20);
}

NSDuckingAudioProcessorEditor::~NSDuckingAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NSDuckingAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto bounds = getLocalBounds();
    g.setColour (juce::Colour { 0xff0f1113u });
    g.fillRect (bounds.removeFromTop (48));

    const auto level          = audioProcessor.getMidiActivity();
    const auto indicatorBounds = midiIndicator.getBounds().toFloat();
    g.setColour (level > 0.0f ? hotAccent.withAlpha (0.35f + 0.65f * level) : juce::Colour { 0xff343a40u });
    g.fillEllipse (indicatorBounds);
    g.setColour (juce::Colour { 0x66000000u });
    g.drawEllipse (indicatorBounds, 1.0f);

    if (currentTab != 0)
    {
        auto pagePanel = getLocalBounds().reduced (22);
        pagePanel.removeFromTop (54);
        g.setColour (panel);
        g.fillRoundedRectangle (pagePanel.toFloat(), 8.0f);
    }
}

void NSDuckingAudioProcessorEditor::resized()
{
    auto area   = getLocalBounds();
    auto header = area.removeFromTop (48).reduced (14, 6);

    logoLabel.setBounds (header.removeFromLeft (220));
    aboutTab.setBounds (header.removeFromRight (72));
    midiTab.setBounds (header.removeFromRight (64).reduced (4, 0));
    mainTab.setBounds (header.removeFromRight (70).reduced (4, 0));
    const auto indicatorSlot = header.removeFromRight (18);
    midiIndicator.setBounds (indicatorSlot.getCentreX() - 6, indicatorSlot.getCentreY() - 6, 12, 12);

    area.reduce (16, 14);

    const auto showMain  = currentTab == 0;
    const auto showMidi  = currentTab == 1;
    const auto showAbout = currentTab == 2;

    // ── Main page layout ─────────────────────────────────────────────────────
    auto bottom   = area.removeFromBottom (112);
    auto main     = area;
    auto controls = main.removeFromLeft (juce::jlimit (160, 235, getWidth() / 3)).reduced (0, 4);
    auto curve    = main.reduced (12, 4);

    smoothLabel.setBounds (controls.removeFromTop (18));
    smoothSlider.setBounds (controls.removeFromTop (82));
    controls.removeFromTop (6);
    depthLabel.setBounds (controls.removeFromTop (18));
    depthSlider.setBounds (controls.removeFromTop (juce::jmax (118, controls.getHeight() - 8)));

    curveDisplay.setBounds (curve);

    auto bottomControls = bottom.removeFromRight (juce::jlimit (150, 220, getWidth() / 4)).reduced (8, 2);
    lengthLabel.setBounds (bottomControls.removeFromTop (18));
    lengthBox.setBounds (bottomControls.removeFromTop (30));
    bottomControls.removeFromTop (8);
    outputLabel.setBounds (bottomControls.removeFromTop (18));
    outputSlider.setBounds (bottomControls.removeFromTop (44));

    const auto columns     = getWidth() > 840 ? 6 : (getWidth() > 620 ? 4 : 3);
    auto       presets     = bottom.reduced (0, 2);
    const auto rows        = juce::roundToInt (std::ceil (static_cast<double> (presetButtons.size()) / static_cast<double> (columns)));
    const auto buttonHeight = juce::jmax (26, presets.getHeight() / juce::jmax (1, rows));
    for (auto i = 0; i < static_cast<int> (presetButtons.size()); ++i)
    {
        const auto row         = i / columns;
        const auto col         = i % columns;
        const auto buttonWidth = presets.getWidth() / columns;
        presetButtons[static_cast<size_t> (i)]->setBounds (
            presets.getX() + col * buttonWidth,
            presets.getY() + row * buttonHeight,
            buttonWidth - 6, buttonHeight - 6);
    }

    // ── MIDI page layout ──────────────────────────────────────────────────────
    {
        auto midiArea = getLocalBounds();
        midiArea.removeFromTop (54);
        midiArea = midiArea.reduced (36, 18);

        const int rowH    = 28;
        const int labelH  = 18;
        const int gapV    = 16;
        const int colW    = juce::jmin (320, midiArea.getWidth() / 2 - 16);

        auto leftCol  = midiArea.removeFromLeft (colW);
        midiArea.removeFromLeft (24);
        auto rightCol = midiArea.removeFromLeft (colW);

        // Left column: Trigger Mode + Velocity Sensitivity
        triggerModeLabel.setBounds (leftCol.removeFromTop (labelH));
        triggerModeCombo.setBounds (leftCol.removeFromTop (rowH));
        leftCol.removeFromTop (gapV);
        velocitySensLabel.setBounds (leftCol.removeFromTop (labelH));
        velocitySensSlider.setBounds (leftCol.removeFromTop (rowH));

        // Right column: Retrigger + Note Filter
        retriggerLabel.setBounds (rightCol.removeFromTop (labelH));
        retriggerCombo.setBounds (rightCol.removeFromTop (rowH));
        rightCol.removeFromTop (gapV);

        noteFilterLabel.setBounds (rightCol.removeFromTop (labelH));
        noteFilterButton.setBounds (rightCol.removeFromTop (rowH).removeFromLeft (juce::jmin (110, rightCol.getWidth())));
        rightCol.removeFromTop (4);
        noteFilterCombo.setBounds (rightCol.removeFromTop (rowH));
    }

    // ── About page layout ─────────────────────────────────────────────────────
    {
        auto aboutArea = getLocalBounds();
        aboutArea.removeFromTop (54);
        aboutPageLabel.setBounds (aboutArea.reduced (36, 36));
    }

    // ── Visibility ────────────────────────────────────────────────────────────
    depthSlider.setVisible (showMain);
    smoothSlider.setVisible (showMain);
    outputSlider.setVisible (showMain);
    lengthBox.setVisible (showMain);
    depthLabel.setVisible (showMain);
    smoothLabel.setVisible (showMain);
    outputLabel.setVisible (showMain);
    lengthLabel.setVisible (showMain);
    curveDisplay.setVisible (showMain);
    for (auto& button : presetButtons)
        button->setVisible (showMain);

    triggerModeLabel.setVisible (showMidi);
    triggerModeCombo.setVisible (showMidi);
    velocitySensLabel.setVisible (showMidi);
    velocitySensSlider.setVisible (showMidi);
    retriggerLabel.setVisible (showMidi);
    retriggerCombo.setVisible (showMidi);
    noteFilterLabel.setVisible (showMidi);
    noteFilterButton.setVisible (showMidi);
    noteFilterCombo.setVisible (showMidi);

    aboutPageLabel.setVisible (showAbout);
}

void NSDuckingAudioProcessorEditor::configureRotarySlider (juce::Slider& slider, juce::Label& label,
                                                            const juce::String& labelText, bool large)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, large ? 74 : 62, 20);
    slider.setColour (juce::Slider::textBoxTextColourId,       text);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour { 0x00000000u });
    slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour { 0x00000000u });
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setFont (labelFont());
    label.setColour (juce::Label::textColourId, mutedText);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void NSDuckingAudioProcessorEditor::configureHorizontalSlider (juce::Slider& slider, juce::Label& label,
                                                                const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
    slider.setColour (juce::Slider::textBoxTextColourId,       text);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour { 0x00000000u });
    slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colour { 0x00000000u });
    addChildComponent (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setFont (labelFont());
    label.setColour (juce::Label::textColourId, mutedText);
    addChildComponent (label);
}

void NSDuckingAudioProcessorEditor::configureTabButton (juce::TextButton& button, int tabIndex)
{
    button.setClickingTogglesState (false);
    button.onClick = [this, tabIndex] { setCurrentTab (tabIndex); };
    addAndMakeVisible (button);
}

void NSDuckingAudioProcessorEditor::setCurrentTab (int tabIndex)
{
    currentTab = juce::jlimit (0, 2, tabIndex);

    if (auto* parameter = choiceParam<juce::AudioParameterChoice> (audioProcessor.getState(), tabId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (currentTab)));
        parameter->endChangeGesture();
    }

    // Update About page text (here so it's always fresh)
    aboutPageLabel.setText (
        "NSDucking  v0.1.0\n\n"
        "Free MIDI-triggered ducking plugin\n"
        "by nisesimadao\n\n"
        "Trigger your audio ducking envelope with MIDI Note On.\n"
        "Edit curves by dragging the handles in the Curve Editor.\n\n"
        "Built with JUCE  --  MIT License\n"
        "github.com/nisesimadao/NSDucking",
        juce::dontSendNotification);

    updateTabButtons();
    resized();
    repaint();
}

void NSDuckingAudioProcessorEditor::updateTabButtons()
{
    mainTab.setToggleState  (currentTab == 0, juce::dontSendNotification);
    midiTab.setToggleState  (currentTab == 1, juce::dontSendNotification);
    aboutTab.setToggleState (currentTab == 2, juce::dontSendNotification);
}

void NSDuckingAudioProcessorEditor::updatePresetButtons()
{
    auto selected = 0;
    if (auto* parameter = choiceParam<juce::AudioParameterChoice> (audioProcessor.getState(), presetId))
        selected = parameter->getIndex();

    for (auto i = 0; i < static_cast<int> (presetButtons.size()); ++i)
        presetButtons[static_cast<size_t> (i)]->setToggleState (i == selected, juce::dontSendNotification);
}

void NSDuckingAudioProcessorEditor::timerCallback()
{
    updatePresetButtons();
    repaint (midiIndicator.getBounds().expanded (2));
}
