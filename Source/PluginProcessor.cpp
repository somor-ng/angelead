#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
using FP = juce::AudioParameterFloat;
using CP = juce::AudioParameterChoice;
static std::unique_ptr<FP> F (const char* id, const char* name, float lo, float hi, float def, float skew = 1.f, float step = 0.f)
{ return std::make_unique<FP> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, step, skew), def); }
static std::unique_ptr<CP> C (const char* id, const char* name, juce::StringArray items, int def = 0)
{ return std::make_unique<CP> (juce::ParameterID { id, 1 }, name, items, def); }

static const juce::StringArray waveNames { "Saw", "Square", "Triangle", "Sine", "Noise", "NES Tri", "Pulse 25" };
static const juce::StringArray arpNames  { "Off", "Octaves", "Major", "Minor", "Power", "Scale" };
static const std::vector<std::vector<int>> arpPatterns {
    { 0, 12 }, { 0, 4, 7, 12 }, { 0, 3, 7, 12 }, { 0, 7, 12, 7 }, { 0, 2, 4, 7, 9, 7, 4, 2 } };

juce::AudioProcessorValueTreeState::ParameterLayout AngeleadProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // oscillators
    l.add (C ("wave1", "Wave 1", waveNames, 0));
    l.add (C ("wave2", "Wave 2", waveNames, 0));
    l.add (F ("pw",     "Pulse Width", 0.05f, 0.95f, 0.5f));
    l.add (F ("mix2",   "Osc 2 Level", 0.f, 1.f, 0.f));
    l.add (F ("semi2",  "Osc 2 Semi", -24.f, 24.f, 0.f, 1.f, 1.f));
    l.add (F ("detune", "Detune", 0.f, 60.f, 8.f));
    l.add (F ("width",  "Width", 0.f, 1.f, 0.5f));
    l.add (F ("glide",  "Glide", 0.f, 1.f, 0.f, 0.5f));
    // envelope
    l.add (F ("attack",  "Attack",  0.001f, 5.f, 0.005f, 0.3f));
    l.add (F ("decay",   "Decay",   0.005f, 5.f, 0.3f, 0.3f));
    l.add (F ("sustain", "Sustain", 0.f, 1.f, 0.7f));
    l.add (F ("release", "Release", 0.005f, 8.f, 0.25f, 0.3f));
    // filter
    l.add (C ("ftype",  "Filter", { "Low", "Band", "High" }, 0));
    l.add (F ("cutoff", "Cutoff", 20.f, 20000.f, 8000.f, 0.25f));
    l.add (F ("res",    "Resonance", 0.f, 1.f, 0.2f));
    l.add (F ("fenv",   "Filter Env", -1.f, 1.f, 0.3f));
    // modulation
    l.add (F ("lfoRate",   "LFO Rate", 0.1f, 20.f, 4.f, 0.4f));
    l.add (F ("lfoPitch",  "LFO > Pitch", 0.f, 1.f, 0.f));
    l.add (F ("lfoFilter", "LFO > Filter", 0.f, 1.f, 0.f));
    l.add (C ("arp",     "Arp", arpNames, 0));
    l.add (F ("arpRate", "Arp Rate", 2.f, 30.f, 12.f));
    // fx
    l.add (F ("drive",    "Drive", 0.f, 1.f, 0.f));
    l.add (F ("crush",    "Bit Crush", 1.f, 16.f, 16.f, 1.f, 0.5f));
    l.add (F ("rate",     "Downsample", 1.f, 40.f, 1.f, 0.5f));
    l.add (F ("chorus",   "Chorus", 0.f, 1.f, 0.f));
    l.add (F ("delayMix", "Delay", 0.f, 1.f, 0.f));
    l.add (F ("delayTime","Delay Time", 0.05f, 1.f, 0.35f));
    l.add (F ("delayFb",  "Feedback", 0.f, 0.95f, 0.35f));
    l.add (F ("reverbMix","Reverb", 0.f, 1.f, 0.f));
    l.add (F ("reverbSize","Size", 0.f, 1.f, 0.6f));
    l.add (F ("gain",     "Volume", 0.f, 1.f, 0.7f));
    return l;
}

//==============================================================================
AngeleadProcessor::AngeleadProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANGELEAD", createLayout())
{
    buildPresets();
}

void AngeleadProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    for (auto& v : voices) v = Voice();
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 2 };
    chorus.prepare (spec); chorus.reset();
    chorus.setRate (0.8f); chorus.setDepth (0.35f); chorus.setCentreDelay (9.f); chorus.setFeedback (0.1f);
    reverb.reset();
    for (auto& b : delayBuf) b.assign ((size_t) (sr * 1.1) + 8, 0.f);
    delayPos = 0;
}

//==============================================================================
// oscillator helpers
static inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.f;
}

static float oscillator (int type, double& ph, double dt, float pw, juce::Random& rng, float& lastNoise)
{
    float out = 0.f;
    switch (type)
    {
        case 0: out = (float) (2.0 * ph - 1.0) - polyBlep (ph, dt); break;
        case 1: { out = ph < pw ? 1.f : -1.f; out += polyBlep (ph, dt);
                  double t2 = ph + (1.0 - pw); if (t2 >= 1.0) t2 -= 1.0; out -= polyBlep (t2, dt); } break;
        case 2: out = (float) (1.0 - 4.0 * std::abs (ph - 0.5)); break;
        case 3: out = std::sin ((float) (juce::MathConstants<double>::twoPi * ph)); break;
        case 4: { if (ph < dt * 2.0 || std::abs (lastNoise) < 1e-6f) lastNoise = rng.nextFloat() * 2.f - 1.f;
                  out = rng.nextFloat() * 2.f - 1.f; } break;
        case 5: { float tri = (float) (1.0 - 4.0 * std::abs (ph - 0.5));
                  out = std::floor ((tri + 1.f) * 7.5f + 0.5f) / 7.5f - 1.f; } break;
        case 6: { out = ph < 0.25 ? 1.f : -1.f; out += polyBlep (ph, dt);
                  double t2 = ph + 0.75; if (t2 >= 1.0) t2 -= 1.0; out -= polyBlep (t2, dt); } break;
    }
    ph += dt; if (ph >= 1.0) ph -= 1.0;
    return out;
}

//==============================================================================
void AngeleadProcessor::noteOn (int note, float vel)
{
    Voice* v = nullptr;
    for (auto& x : voices) if (! x.active) { v = &x; break; }
    if (! v) { v = &voices[0]; for (auto& x : voices) if (x.age < v->age) v = &x; }

    double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    v->active = true; v->released = false; v->note = note; v->vel = vel; v->stage = 0;
    v->targetFreq = f;
    v->freq = (s.glide > 0.0005f && lastFreq > 0) ? lastFreq : f;
    lastFreq = f;
    v->ph1 = v->ph2 = 0; v->env = 0; v->ic1 = v->ic2 = 0; v->arpStep = 0; v->arpTimer = 0;
    v->age = ++voiceCounter;
}

void AngeleadProcessor::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && ! v.released && v.note == note) { v.released = true; v.stage = 3; }
}

void AngeleadProcessor::renderVoice (Voice& v, float& oL, float& oR)
{
    if (s.glide > 0.0005f)
        v.freq += (v.targetFreq - v.freq) * (1.0 - std::exp (-1.0 / (s.glide * sr * 0.3)));
    else v.freq = v.targetFreq;

    double semi = s.lfo * s.lfoPitch * 1.5;
    if (s.arp > 0 && ! v.released)
    {
        v.arpTimer += s.arpRate / sr;
        if (v.arpTimer >= 1.0) { v.arpTimer -= 1.0; ++v.arpStep; }
        auto& pat = arpPatterns[(size_t) (s.arp - 1)];
        semi += pat[(size_t) v.arpStep % pat.size()];
    }
    const double f  = v.freq * std::pow (2.0, semi / 12.0);
    const double d1 = f * std::pow (2.0, (-0.5 * s.detune) / 1200.0) / sr;
    const double d2 = f * std::pow (2.0, (s.semi2 * 100.0 + 0.5 * s.detune) / 1200.0) / sr;

    // envelope
    switch (v.stage)
    {
        case 0: v.env += 1.f / (std::max (0.0005f, s.a) * (float) sr); if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } break;
        case 1: v.env += (s.sus - v.env) * (float) (1.0 - std::exp (-1.0 / (s.d * sr * 0.35)));
                if (std::abs (v.env - s.sus) < 0.001f) v.stage = 2; break;
        case 2: v.env = s.sus; break;
        case 3: v.env -= v.env * (float) (1.0 - std::exp (-1.0 / (s.r * sr * 0.35)));
                if (v.env < 0.0004f) { v.active = false; return; } break;
    }
    if (v.stage != 3 && v.stage != 0 && s.sus < 0.001f && v.env < 0.0004f) { v.active = false; return; }

    float o1 = oscillator (s.w1, v.ph1, d1, s.pw, rng, lastNoise);
    float o2 = s.mix2 > 0.001f ? oscillator (s.w2, v.ph2, d2, s.pw, rng, lastNoise) * s.mix2 : 0.f;

    // filter (TPT state-variable), modulated by envelope + LFO
    float fc = s.cutoff * std::pow (2.f, v.env * s.fenv * 5.f + s.lfo * s.lfoFilter * 2.f);
    fc = juce::jlimit (20.f, (float) std::min (18000.0, sr * 0.45), fc);
    const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
    const float k = 2.f - 1.95f * s.res;
    const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float x = o1 + o2;
    const float v3 = x - v.ic2;
    const float v1 = a1 * v.ic1 + a2 * v3;
    const float v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
    v.ic1 = 2.f * v1 - v.ic1; v.ic2 = 2.f * v2 - v.ic2;
    float y = s.ftype == 0 ? v2 : (s.ftype == 1 ? v1 * k : x - k * v1 - v2);

    y *= v.env * v.vel * 0.3f;
    // simple stereo spread from note-dependent pan
    const float pan = ((v.note * 37) % 13) / 12.f - 0.5f;               // -0.5..0.5
    oL += y * (1.f - std::max (0.f,  pan) * s.width * 1.6f);
    oR += y * (1.f - std::max (0.f, -pan) * s.width * 1.6f);
}

void AngeleadProcessor::renderRange (juce::AudioBuffer<float>& buf, int start, int end)
{
    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);
    const float lfoInc = *p ("lfoRate") / (float) sr;
    for (int i = start; i < end; ++i)
    {
        lfoPhase += lfoInc; if (lfoPhase >= 1.f) lfoPhase -= 1.f;
        s.lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
        float l = 0, r = 0;
        for (auto& v : voices) if (v.active) renderVoice (v, l, r);
        L[i] += l; R[i] += r;
    }
}

void AngeleadProcessor::applyFx (juce::AudioBuffer<float>& buf)
{
    const int n = buf.getNumSamples();
    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);

    const float drive = *p ("drive"), bits = *p ("crush"), rate = *p ("rate");
    const float dMix = *p ("delayMix"), dFb = *p ("delayFb");
    const int   dSamp = juce::jlimit (1, (int) delayBuf[0].size() - 1, (int) (*p ("delayTime") * sr));
    const float rMix = *p ("reverbMix"), chMix = *p ("chorus"), gain = *p ("gain");
    const float step = std::pow (2.f, bits - 1.f);

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (drive > 0.001f) { const float gd = 1.f + drive * 9.f; l = std::tanh (l * gd) / std::sqrt (gd); r = std::tanh (r * gd) / std::sqrt (gd); }
        if (bits < 15.9f)   { l = std::round (l * step) / step; r = std::round (r * step) / step; }
        if (rate > 1.01f)   { crushAcc += 1.f; if (crushAcc >= rate) { crushAcc -= rate; crushHoldL = l; crushHoldR = r; } l = crushHoldL; r = crushHoldR; }
        L[i] = l; R[i] = r;
    }

    if (chMix > 0.001f)
    {
        chorus.setMix (chMix * 0.6f);
        juce::dsp::AudioBlock<float> block (buf);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    if (dMix > 0.001f)
    {
        const int size = (int) delayBuf[0].size();
        for (int i = 0; i < n; ++i)
        {
            const int rp = (delayPos - dSamp + size) % size;
            const float dl = delayBuf[0][(size_t) rp], dr = delayBuf[1][(size_t) rp];
            delayBuf[0][(size_t) delayPos] = L[i] + dr * dFb;      // ping-pong
            delayBuf[1][(size_t) delayPos] = R[i] + dl * dFb;
            L[i] += dl * dMix; R[i] += dr * dMix;
            delayPos = (delayPos + 1) % size;
        }
    }
    else for (auto& b : delayBuf) std::fill (b.begin(), b.end(), 0.f);

    if (rMix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = *p ("reverbSize"); rp.damping = 0.45f; rp.width = 1.f;
        rp.wetLevel = rMix * 0.5f; rp.dryLevel = 1.f - rMix * 0.25f; rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);
    }

    int pos = scopePos.load();
    for (int i = 0; i < n; ++i)
    {
        L[i] *= gain; R[i] *= gain;
        if ((i & 3) == 0) { scope[(size_t) pos] = L[i]; pos = (pos + 1) % scopeSize; }
    }
    scopePos.store (pos);
}

void AngeleadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    s.w1 = (int) *p ("wave1"); s.w2 = (int) *p ("wave2"); s.ftype = (int) *p ("ftype"); s.arp = (int) *p ("arp");
    s.pw = *p ("pw"); s.mix2 = *p ("mix2"); s.semi2 = *p ("semi2"); s.detune = *p ("detune"); s.width = *p ("width");
    s.glide = *p ("glide"); s.a = *p ("attack"); s.d = *p ("decay"); s.sus = *p ("sustain"); s.r = *p ("release");
    s.cutoff = *p ("cutoff"); s.res = *p ("res"); s.fenv = *p ("fenv");
    s.lfoPitch = *p ("lfoPitch"); s.lfoFilter = *p ("lfoFilter"); s.arpRate = *p ("arpRate"); s.gain = *p ("gain");

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { renderRange (buffer, pos, at); pos = at; }
        if (m.isNoteOn())       noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) if (v.active) { v.released = true; v.stage = 3; }
    }
    if (pos < n) renderRange (buffer, pos, n);
    applyFx (buffer);
}

//==============================================================================
void AngeleadProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void AngeleadProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AngeleadProcessor::createEditor() { return new AngeleadEditor (*this); }

//==============================================================================
void AngeleadProcessor::setPlain (const juce::String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

void AngeleadProcessor::applyPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        prm->setValueNotifyingHost (prm->getDefaultValue());
    for (auto& kv : presets[(size_t) index].values) setPlain (kv.first, kv.second);
}

void AngeleadProcessor::randomise()
{
    auto r = [this] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    setPlain ("wave1", (float) rng.nextInt (7));  setPlain ("wave2", (float) rng.nextInt (7));
    setPlain ("mix2", r (0.f, 0.8f));             setPlain ("semi2", (float) (rng.nextBool() ? 12 : (rng.nextBool() ? 7 : 0)));
    setPlain ("detune", r (0.f, 30.f));           setPlain ("pw", r (0.2f, 0.8f));
    setPlain ("attack", std::pow (r (0.f, 1.f), 3.f) * 2.f + 0.002f);
    setPlain ("decay", r (0.1f, 1.5f));           setPlain ("sustain", r (0.f, 1.f));
    setPlain ("release", r (0.1f, 2.5f));
    setPlain ("cutoff", std::pow (r (0.f, 1.f), 2.f) * 12000.f + 600.f);
    setPlain ("res", r (0.f, 0.6f));              setPlain ("fenv", r (-0.2f, 0.9f));
    setPlain ("lfoRate", r (0.2f, 8.f));          setPlain ("lfoFilter", r (0.f, 0.5f));
    setPlain ("chorus", r (0.f, 0.6f));           setPlain ("delayMix", r (0.f, 0.4f));
    setPlain ("reverbMix", r (0.f, 0.6f));
}

//==============================================================================
void AngeleadProcessor::buildPresets()
{
    auto add = [this] (const char* cat, const char* name, std::vector<std::pair<juce::String, float>> v)
    { presets.push_back ({ name, cat, std::move (v) }); };
    // waves: 0 Saw 1 Square 2 Tri 3 Sine 4 Noise 5 NES 6 Pulse25 | filter: 0 Low 1 Band 2 High
    // arp: 0 Off 1 Oct 2 Major 3 Minor 4 Power 5 Scale

    // ---- LEAD
    add ("Lead", "Neon Saw",   { {"wave1",0},{"wave2",0},{"mix2",.7f},{"detune",14},{"cutoff",9000},{"res",.25f},{"fenv",.3f},{"glide",.08f},{"attack",.005f},{"release",.25f},{"chorus",.35f},{"delayMix",.2f},{"reverbMix",.15f} });
    add ("Lead", "Soft Sine",  { {"wave1",3},{"wave2",2},{"mix2",.4f},{"semi2",12},{"cutoff",12000},{"glide",.12f},{"attack",.01f},{"release",.35f},{"lfoPitch",.08f},{"lfoRate",5.5f},{"reverbMix",.3f} });
    add ("Lead", "Angel Voice",{ {"wave1",0},{"wave2",2},{"mix2",.6f},{"detune",16},{"cutoff",5200},{"res",.15f},{"attack",.06f},{"release",.6f},{"chorus",.55f},{"delayMix",.25f},{"reverbMix",.4f} });
    add ("Lead", "Hyper Square",{ {"wave1",1},{"wave2",1},{"mix2",.5f},{"semi2",12},{"pw",.4f},{"cutoff",7000},{"res",.35f},{"fenv",.5f},{"glide",.03f},{"delayMix",.2f},{"delayFb",.45f} });
    // ---- PLUCK
    add ("Pluck", "Glass Pluck",{ {"wave1",2},{"wave2",3},{"mix2",.6f},{"semi2",12},{"attack",.001f},{"decay",.35f},{"sustain",0},{"release",.4f},{"cutoff",3200},{"fenv",.6f},{"delayMix",.25f},{"reverbMix",.3f} });
    add ("Pluck", "Bright Pluck",{ {"wave1",0},{"attack",.001f},{"decay",.25f},{"sustain",0},{"release",.2f},{"cutoff",1500},{"res",.3f},{"fenv",.9f},{"delayMix",.2f} });
    add ("Pluck", "Marimba",   { {"wave1",3},{"wave2",3},{"mix2",.5f},{"semi2",19},{"attack",.001f},{"decay",.45f},{"sustain",0},{"release",.5f},{"cutoff",6000},{"reverbMix",.25f} });
    add ("Pluck", "Soft Harp", { {"wave1",2},{"wave2",0},{"mix2",.3f},{"semi2",12},{"attack",.002f},{"decay",.9f},{"sustain",0},{"release",.9f},{"cutoff",2600},{"fenv",.4f},{"chorus",.4f},{"reverbMix",.45f} });
    // ---- AMBIENT
    add ("Ambient", "Drifting Light",{ {"wave1",3},{"wave2",0},{"mix2",.4f},{"detune",20},{"attack",1.5f},{"decay",1.f},{"sustain",.8f},{"release",3.5f},{"cutoff",2500},{"fenv",0},{"lfoRate",.3f},{"lfoFilter",.3f},{"chorus",.5f},{"delayMix",.4f},{"delayTime",.5f},{"delayFb",.6f},{"reverbMix",.8f},{"reverbSize",.9f},{"width",1} });
    add ("Ambient", "Glass Air",   { {"wave1",2},{"wave2",3},{"mix2",.6f},{"semi2",12},{"attack",.9f},{"sustain",.9f},{"release",3.f},{"cutoff",4500},{"fenv",0},{"lfoRate",.6f},{"lfoPitch",.05f},{"chorus",.6f},{"delayMix",.35f},{"reverbMix",.75f},{"reverbSize",.85f},{"width",1} });
    add ("Ambient", "Dust & Wind", { {"wave1",4},{"wave2",3},{"mix2",.3f},{"attack",1.2f},{"sustain",.8f},{"release",3.f},{"ftype",1},{"cutoff",1400},{"res",.5f},{"fenv",0},{"lfoRate",.25f},{"lfoFilter",.6f},{"reverbMix",.7f},{"reverbSize",.9f},{"delayMix",.3f} });
    // ---- PADS
    add ("Pads", "Warm Pad",   { {"wave1",0},{"wave2",0},{"mix2",.8f},{"detune",22},{"attack",.8f},{"sustain",.85f},{"release",1.8f},{"cutoff",1800},{"fenv",.1f},{"chorus",.6f},{"reverbMix",.5f},{"width",.9f} });
    add ("Pads", "Dark Pad",   { {"wave1",0},{"wave2",1},{"mix2",.5f},{"detune",12},{"attack",1.f},{"sustain",.9f},{"release",2.5f},{"cutoff",900},{"res",.3f},{"fenv",.15f},{"lfoRate",.2f},{"lfoFilter",.25f},{"chorus",.4f},{"reverbMix",.6f},{"reverbSize",.8f} });
    add ("Pads", "Soft Choir", { {"wave1",2},{"wave2",0},{"mix2",.5f},{"detune",18},{"attack",.6f},{"sustain",.8f},{"release",1.6f},{"ftype",1},{"cutoff",2200},{"res",.4f},{"lfoPitch",.04f},{"lfoRate",5.f},{"chorus",.7f},{"reverbMix",.55f} });
    add ("Pads", "Rose Strings",{ {"wave1",0},{"wave2",2},{"mix2",.6f},{"detune",15},{"attack",.35f},{"sustain",.8f},{"release",1.2f},{"cutoff",3200},{"fenv",.3f},{"chorus",.5f},{"reverbMix",.4f} });
    // ---- RETRO
    add ("Retro", "NES Pulse",  { {"wave1",1},{"pw",.5f},{"mix2",0},{"detune",0},{"cutoff",16000},{"fenv",0},{"attack",.001f},{"decay",.15f},{"sustain",.6f},{"release",.06f},{"crush",6},{"rate",3},{"width",0} });
    add ("Retro", "Chip Arp",   { {"wave1",6},{"wave2",1},{"mix2",.4f},{"semi2",12},{"cutoff",14000},{"fenv",0},{"arp",2},{"arpRate",14},{"attack",.001f},{"sustain",.7f},{"release",.08f},{"crush",7},{"rate",2},{"delayMix",.25f},{"delayTime",.21f} });
    add ("Retro", "8-bit Bass", { {"wave1",5},{"wave2",1},{"mix2",.3f},{"semi2",-12},{"pw",.3f},{"cutoff",3500},{"fenv",.2f},{"attack",.001f},{"decay",.2f},{"sustain",.8f},{"release",.08f},{"crush",5},{"rate",2},{"drive",.2f} });
    add ("Retro", "Boss Fight", { {"wave1",0},{"wave2",1},{"mix2",.5f},{"detune",10},{"cutoff",6000},{"res",.35f},{"arp",4},{"arpRate",16},{"attack",.001f},{"sustain",.8f},{"release",.1f},{"drive",.5f},{"crush",6},{"rate",3},{"delayMix",.2f} });
    add ("Retro", "Coin Pickup",{ {"wave1",1},{"pw",.5f},{"cutoff",16000},{"fenv",0},{"arp",1},{"arpRate",22},{"attack",.001f},{"decay",.12f},{"sustain",.3f},{"release",.15f},{"crush",5},{"rate",2},{"delayMix",.15f} });
    add ("Retro", "Game Over",  { {"wave1",5},{"wave2",4},{"mix2",.15f},{"glide",.6f},{"cutoff",5000},{"fenv",0},{"attack",.002f},{"decay",.4f},{"sustain",.5f},{"release",.5f},{"crush",5},{"rate",5},{"reverbMix",.2f} });
}
