#include "PluginEditor.h"

static constexpr int W = 960, H = 560;

AngeleadEditor::AngeleadEditor (AngeleadProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&laf);
    laf.h = (float) H;
    girl = juce::ImageCache::getFromMemory (BinaryData::angelead_girl_png, BinaryData::angelead_girl_pngSize);

    // ---- header: category tabs + preset browser
    for (auto& c : categories)
    {
        auto* b = catButtons.add (new juce::TextButton (c));
        b->setClickingTogglesState (false);
        b->onClick = [this, c] { selectCategory (c); };
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff111111));
        addAndMakeVisible (b);
    }
    for (auto* b : { &prevBtn, &nextBtn, &randBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff111111));
        addAndMakeVisible (b);
    }
    randBtn.setButtonText ("Random");
    prevBtn.onClick = [this] { showPreset (-1); };
    nextBtn.onClick = [this] { showPreset (+1); };
    randBtn.onClick = [this] { proc.randomise(); };
    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setColour (juce::Label::textColourId, juce::Colour (0xff111111));
    presetLabel.setFont (juce::Font (juce::FontOptions (14.f)));
    addAndMakeVisible (presetLabel);

    // ---- controls
    auto& a = proc.apvts;
    (void) a;
    addChoice ("wave1", "Wave 1"); addChoice ("wave2", "Wave 2");
    addKnob ("mix2", "Osc 2"); addKnob ("semi2", "Semi"); addKnob ("detune", "Detune"); addKnob ("pw", "Pulse W");
    addKnob ("width", "Width"); addKnob ("glide", "Glide");

    addKnob ("attack", "Attack"); addKnob ("decay", "Decay"); addKnob ("sustain", "Sustain"); addKnob ("release", "Release");

    addChoice ("ftype", "Filter"); addKnob ("cutoff", "Cutoff"); addKnob ("res", "Reso"); addKnob ("fenv", "Env Amt");

    addKnob ("lfoRate", "LFO Rate"); addKnob ("lfoPitch", "> Pitch"); addKnob ("lfoFilter", "> Filter");
    addChoice ("arp", "Arp"); addKnob ("arpRate", "Arp Rate");

    addKnob ("drive", "Drive"); addKnob ("crush", "Bits"); addKnob ("rate", "Downsmp");
    addKnob ("chorus", "Chorus"); addKnob ("delayMix", "Delay"); addKnob ("delayTime", "Time"); addKnob ("delayFb", "Fdbk");
    addKnob ("reverbMix", "Reverb"); addKnob ("reverbSize", "Size"); addKnob ("gain", "Volume");

    keyboard.setKeyWidth (22.f);
    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffeeeeee));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1a1a1a));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0x33000000));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x22ffffff));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0x88ffffff));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keyboard);

    setSize (W, H);
    selectCategory (proc.getPresets()[(size_t) proc.currentPreset].category);
    presetLabel.setText (proc.getPresets()[(size_t) proc.currentPreset].name, juce::dontSendNotification);
    startTimerHz (30);
}

Knob* AngeleadEditor::addKnob (const char* id, const char* name)
{ auto* k = knobs.add (new Knob (proc.apvts, id, name)); addAndMakeVisible (k); return k; }
Choice* AngeleadEditor::addChoice (const char* id, const char* name)
{ auto* c = choices.add (new Choice (proc.apvts, id, name)); addAndMakeVisible (c); return c; }

void AngeleadEditor::selectCategory (const juce::String& cat)
{
    activeCategory = cat;
    auto& ps = proc.getPresets();
    if (ps[(size_t) proc.currentPreset].category != cat)
        for (int i = 0; i < (int) ps.size(); ++i)
            if (ps[(size_t) i].category == cat) { proc.applyPreset (i); presetLabel.setText (ps[(size_t) i].name, juce::dontSendNotification); break; }
    for (auto* b : catButtons) b->setAlpha (b->getButtonText() == cat ? 1.f : 0.4f);
    repaint();
}

void AngeleadEditor::showPreset (int dir)
{
    auto& ps = proc.getPresets();
    std::vector<int> idx;
    for (int i = 0; i < (int) ps.size(); ++i) if (ps[(size_t) i].category == activeCategory) idx.push_back (i);
    if (idx.empty()) return;
    auto it = std::find (idx.begin(), idx.end(), proc.currentPreset);
    int pos = it == idx.end() ? 0 : (int) (it - idx.begin());
    pos = (pos + dir + (int) idx.size()) % (int) idx.size();
    proc.applyPreset (idx[(size_t) pos]);
    presetLabel.setText (ps[(size_t) idx[(size_t) pos]].name, juce::dontSendNotification);
}

//==============================================================================
void AngeleadEditor::drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t)
{
    const float y = (float) r.getY();
    g.setColour (AngeleadLookAndFeel::inkAt (y, (float) H, 0.9f));
    g.setFont (juce::Font (juce::FontOptions (11.f).withStyle ("Bold")));
    g.drawText (t.toUpperCase(), r.getX(), r.getY(), r.getWidth(), 16, juce::Justification::left);
    // hairline + small diamond ornament (nod to the ornate divider reference)
    g.setColour (AngeleadLookAndFeel::inkAt (y, (float) H, 0.25f));
    g.drawHorizontalLine (r.getY() + 20, (float) r.getX(), (float) r.getRight() - 10.f);
    juce::Path d; auto cx = (float) r.getRight() - 5.f, cy = (float) r.getY() + 20.f;
    d.addQuadrilateral (cx, cy - 3, cx + 3, cy, cx, cy + 3, cx - 3, cy);
    g.setColour (AngeleadLookAndFeel::inkAt (y, (float) H, 0.6f)); g.fillPath (d);
}

void AngeleadEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafafa), 0.f, 0.f, juce::Colour (0xff050505), 0.f, (float) H, false));
    g.fillAll();

    // header
    auto hd = headerArea;
    g.setColour (juce::Colour (0xff111111));
    g.setFont (juce::Font (juce::FontOptions (26.f).withStyle ("Light")));
    g.drawText ("A N G E L E A D", 92, 18, 300, 30, juce::Justification::left);
    g.setColour (juce::Colour (0x88111111));
    g.setFont (juce::Font (juce::FontOptions (10.f)));
    g.drawText ("MINIMAL SYNTH FOR LEADS · PLUCKS · AMBIENTS · PADS · RETRO", 93, 46, 400, 14, juce::Justification::left);

    if (girl.isValid())
    {
        auto box = juce::Rectangle<float> (22.f, 12.f, 56.f, 56.f);
        g.setColour (juce::Colour (0x14000000)); g.fillEllipse (box);
        g.setColour (juce::Colour (0x33000000)); g.drawEllipse (box, 1.f);
        g.saveState();
        juce::Path clip; clip.addEllipse (box); g.reduceClipRegion (clip);
        const float sc = std::max (box.getWidth() / girl.getWidth(), box.getHeight() / girl.getHeight()) * 1.1f;
        g.drawImageTransformed (girl, juce::AffineTransform::scale (sc)
                                    .translated (box.getCentreX() - girl.getWidth() * sc * 0.5f, box.getY() - 2.f), false);
        g.restoreState();
    }

    for (auto& s : sections) drawSectionTitle (g, s.area, s.title);

    // oscilloscope (waveform reference)
    g.setColour (juce::Colour (0x22ffffff));
    g.fillRoundedRectangle (scopeArea.toFloat(), 8.f);
    g.setColour (juce::Colour (0x44ffffff));
    g.drawRoundedRectangle (scopeArea.toFloat().reduced (0.5f), 8.f, 1.f);
    juce::Path wave; const int n = AngeleadProcessor::scopeSize; const int start = proc.scopePos.load();
    const float cy = (float) scopeArea.getCentreY(), amp = scopeArea.getHeight() * 0.42f;
    for (int i = 0; i < n; ++i)
    {
        float v = proc.scope[(size_t) ((start + i) % n)];
        float x = scopeArea.getX() + 6.f + (scopeArea.getWidth() - 12.f) * i / (float) n;
        float y = cy - juce::jlimit (-1.f, 1.f, v * 1.5f) * amp;
        if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
    }
    g.setColour (juce::Colour (0xffeeeeee));
    g.strokePath (wave, juce::PathStrokeType (1.2f));
}

void AngeleadEditor::resized()
{
    headerArea = { 0, 0, W, 80 };
    int x = 380;
    for (auto* b : catButtons) { b->setBounds (x, 20, 62, 26); x += 64; }
    prevBtn.setBounds (W - 330, 50, 26, 22); presetLabel.setBounds (W - 300, 50, 150, 22);
    nextBtn.setBounds (W - 146, 50, 26, 22); randBtn.setBounds (W - 100, 50, 80, 22);

    const int kw = 62, kh = 84;
    auto place = [&] (const juce::String& title, int sx, int sy, int cols, std::vector<juce::Component*> comps)
    {
        sections.push_back ({ title, { sx, sy, cols * kw, 22 } });
        int c = 0, r = 0;
        for (auto* comp : comps)
        {
            comp->setBounds (sx + c * kw, sy + 26 + r * kh, kw, kh);
            if (++c >= cols) { c = 0; ++r; }
        }
    };
    sections.clear();
    auto K = [this] (int i) -> juce::Component* { return knobs[i]; };
    auto C = [this] (int i) -> juce::Component* { return choices[i]; };
    // knob order follows creation order in the constructor
    // 0 mix2 1 semi2 2 detune 3 pw 4 width 5 glide | 6-9 ADSR | 10 cutoff 11 res 12 fenv | 13 lfoRate 14 >pitch 15 >filter 16 arpRate
    // 17 drive 18 crush 19 rate 20 chorus 21 delay 22 time 23 fdbk 24 reverb 25 size 26 volume
    // choices: 0 wave1 1 wave2 2 ftype 3 arp
    place ("Oscillators", 24, 92, 4, { C (0), C (1), K (0), K (1), K (2), K (3), K (4), K (5) });
    place ("Envelope",   284, 92, 4, { K (6), K (7), K (8), K (9) });
    place ("Filter",     544, 92, 4, { C (2), K (10), K (11), K (12) });
    place ("Modulation", 804, 92, 2, { K (13), K (14), K (15), C (3), K (16) });

    place ("Retro / Drive", 24, 290, 3, { K (17), K (18), K (19) });
    place ("Chorus · Delay", 224, 290, 4, { K (20), K (21), K (22), K (23) });
    place ("Reverb",  494, 290, 2, { K (24), K (25) });
    place ("Output",  644, 290, 1, { K (26) });
    scopeArea = { 284, 212, 508, 70 };

    keyboard.setBounds (24, H - 148, W - 48, 124);
}
