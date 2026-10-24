#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"

//==============================================================================
// Minimal look: ink colour flips automatically from dark (on the white top) to
// light (on the black bottom) so every control stays readable on the gradient.
//==============================================================================
struct AngeleadLookAndFeel : public juce::LookAndFeel_V4
{
    static juce::Colour inkAt (float y, float h, float alpha = 1.f)
    {
        return (y / h < 0.5f ? juce::Colour (0xff111111) : juce::Colour (0xfff2f2f2)).withAlpha (alpha);
    }
    float h = 560.f;

    AngeleadLookAndFeel() { setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int hh, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) hh).reduced (8.f);
        auto d = std::min (b.getWidth(), b.getHeight());
        auto c = b.getCentre();
        auto r = d * 0.5f;
        const float cy = (float) s.getScreenY() - (float) s.getTopLevelComponent()->getScreenY() + (float) s.getHeight() * 0.5f;
        auto ink = inkAt (cy, h);
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
        g.setColour (ink.withAlpha (0.18f));
        g.strokePath (track, juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ang = a0 + pos * (a1 - a0);
        val.addCentredArc (c.x, c.y, r, r, 0, a0, ang, true);
        g.setColour (ink);
        g.strokePath (val, juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Point<float> tip (c.x + (r - 6.f) * std::sin (ang), c.y - (r - 6.f) * std::cos (ang));
        g.fillEllipse (tip.x - 2.5f, tip.y - 2.5f, 5.f, 5.f);
    }

    void drawComboBox (juce::Graphics& g, int w, int hh, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const float cy = (float) box.getScreenY() - (float) box.getTopLevelComponent()->getScreenY() + hh * 0.5f;
        g.setColour (inkAt (cy, h, 0.35f));
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.f, (float) hh - 1.f, (float) hh * 0.5f, 1.f);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.f)); }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override { l.setBounds (10, 0, b.getWidth() - 20, b.getHeight()); l.setJustificationType (juce::Justification::centred); }
};

//==============================================================================
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& text)
        : att (apvts, id, slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
        slider.setDoubleClickReturnValue (true, apvts.getParameter (id)->convertFrom0to1 (apvts.getParameter (id)->getDefaultValue()));
        addAndMakeVisible (slider);
        name = text;
        slider.onValueChange = [this] { repaint(); };
    }
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (28)); }
    void paint (juce::Graphics& g) override
    {
        const float y = (float) getScreenY() - (float) getTopLevelComponent()->getScreenY();
        g.setColour (AngeleadLookAndFeel::inkAt (y, 560.f, 0.55f));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (name.toUpperCase(), getLocalBounds().removeFromBottom (28).removeFromTop (14), juce::Justification::centred);
        g.setColour (AngeleadLookAndFeel::inkAt (y, 560.f, 0.9f));
        g.setFont (juce::Font (juce::FontOptions (10.f)));
        g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromBottom (14), juce::Justification::centred);
    }
    juce::Slider slider;
    juce::String name;
    juce::AudioProcessorValueTreeState::SliderAttachment att;
};

struct Choice : public juce::Component
{
    Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& text)
        : name (text)
    {
        box.addItemList (dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id))->choices, 1);
        box.setColour (juce::ComboBox::textColourId, juce::Colour (0xff111111));
        addAndMakeVisible (box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, box);
    }
    void resized() override { box.setBounds (getLocalBounds().withTrimmedBottom (18).withSizeKeepingCentre (getWidth() - 6, 24).withY (6)); }
    void paint (juce::Graphics& g) override
    {
        const float y = (float) getScreenY() - (float) getTopLevelComponent()->getScreenY();
        g.setColour (AngeleadLookAndFeel::inkAt (y, 560.f, 0.55f));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (name.toUpperCase(), getLocalBounds().removeFromBottom (18), juce::Justification::centred);
    }
    juce::ComboBox box;
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

//==============================================================================
class AngeleadEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AngeleadEditor (AngeleadProcessor&);
    ~AngeleadEditor() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint (scopeArea); }
    void showPreset (int);
    void selectCategory (const juce::String&);
    void drawSectionTitle (juce::Graphics&, juce::Rectangle<int>, const juce::String&);

    AngeleadProcessor& proc;
    AngeleadLookAndFeel laf;
    juce::Image girl;

    juce::Rectangle<int> scopeArea, headerArea;
    juce::StringArray categories { "Lead", "Pluck", "Ambient", "Pads", "Retro" };
    juce::String activeCategory { "Lead" };
    juce::OwnedArray<juce::TextButton> catButtons;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, randBtn { "dice" };
    juce::Label presetLabel;

    struct Section { juce::String title; juce::Rectangle<int> area; };
    std::vector<Section> sections;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Choice> choices;
    Knob* addKnob (const char* id, const char* name);
    Choice* addChoice (const char* id, const char* name);
    std::vector<std::pair<juce::Component*, juce::String>> sectionOf;

    juce::MidiKeyboardComponent keyboard;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AngeleadEditor)
};
