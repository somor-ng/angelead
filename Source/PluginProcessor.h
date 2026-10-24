#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
//  ANGELEAD – minimal polyphonic synth: leads, plucks, ambients, pads, retro
//==============================================================================
struct Preset
{
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> values;   // paramID -> plain value
};

class AngeleadProcessor : public juce::AudioProcessor
{
public:
    AngeleadProcessor();
    ~AngeleadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Angelead"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- presets
    const std::vector<Preset>& getPresets() const { return presets; }
    void applyPreset (int index);
    void randomise();
    int currentPreset = 0;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // scope ring buffer (read by the editor)
    static constexpr int scopeSize = 1024;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopePos { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void buildPresets();
    void setPlain (const juce::String& id, float plain);

    struct Voice
    {
        bool active = false, released = false;
        int note = 0; float vel = 0.f;
        double ph1 = 0, ph2 = 0, freq = 440, targetFreq = 440;
        float env = 0.f; int stage = 0;           // 0 A, 1 D, 2 S, 3 R
        float ic1 = 0.f, ic2 = 0.f;               // filter state
        double arpTimer = 0; int arpStep = 0;
        double age = 0;
    };
    static constexpr int numVoices = 12;
    std::array<Voice, numVoices> voices;

    void noteOn (int note, float vel);
    void noteOff (int note);
    void renderVoice (Voice&, float& outL, float& outR);

    // cached params (raw atomic pointers)
    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }

    double sr = 44100.0;
    juce::Random rng;
    float lfoPhase = 0.f;
    float lastNoise = 0.f;

    struct Snap {
        int w1=0,w2=0,ftype=0,arp=0;
        float pw=.5f,mix2=0,semi2=0,detune=0,width=.5f,glide=0,a=.01f,d=.3f,sus=.7f,r=.3f,
              cutoff=8000,res=.2f,fenv=0,lfoPitch=0,lfoFilter=0,arpRate=12,gain=.7f,lfo=0;
    } s;
    double lastFreq = 0, voiceCounter = 0;
    void renderRange (juce::AudioBuffer<float>&, int start, int end);
    void applyFx (juce::AudioBuffer<float>&);
    float crushAcc = 0;

    // fx
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;
    std::array<std::vector<float>, 2> delayBuf;
    int delayPos = 0;
    float crushHoldL = 0, crushHoldR = 0;

    std::vector<Preset> presets;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AngeleadProcessor)
};
