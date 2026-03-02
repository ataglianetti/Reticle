#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class ReticleProcessor : public juce::AudioProcessor
{
public:
    ReticleProcessor();
    ~ReticleProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // --- Metering data (thread-safe, read by editor) ---
    std::atomic<float> peakLevelL { 0.0f };
    std::atomic<float> peakLevelR { 0.0f };
    std::atomic<float> rmsLevelL  { 0.0f };
    std::atomic<float> rmsLevelR  { 0.0f };

    // --- Tunable parameters (written by editor, read by editor) ---
    std::atomic<float> attackCoeff  { 0.3f };   // 0.01 - 1.0 (fast attack)
    std::atomic<float> decayCoeff   { 0.95f };  // 0.8 - 0.999 (slow decay)
    std::atomic<float> peakHoldSec  { 1.0f };   // 0.0 - 5.0 seconds
    std::atomic<float> dbFloor      { -60.0f }; // -96 to -24
    std::atomic<int>   refreshRate  { 30 };     // 15 - 60 fps

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReticleProcessor)
};
