#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>

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

    // --- Level metering (read by UI) ---
    std::atomic<float> peakLevelL { 0.0f };
    std::atomic<float> peakLevelR { 0.0f };
    std::atomic<float> rmsLevelL  { 0.0f };
    std::atomic<float> rmsLevelR  { 0.0f };

    // --- FFT spectrum (read by UI) ---
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder; // 2048
    static constexpr int fftBins  = fftSize / 2;

    // Double-buffer: processor writes to [writeBank], UI reads latest completed
    std::array<float, fftBins> spectrumBanks[2] {};
    std::atomic<int>  spectrumWriteBank { 0 };
    std::atomic<bool> spectrumReady { false };

    // --- LUFS loudness (read by UI) ---
    std::atomic<float> lufsMomentary  { -100.0f };
    std::atomic<float> lufsShortTerm  { -100.0f };

    // Integrated loudness: computed by UI thread from gating block snapshot
    float computeGatedIntegrated() const;
    void resetIntegratedLoudness();

    // --- Tunable parameters ---
    std::atomic<float> attackCoeff  { 0.3f };
    std::atomic<float> decayCoeff   { 0.95f };
    std::atomic<float> peakHoldSec  { 1.0f };
    std::atomic<float> dbFloor      { -60.0f };
    std::atomic<int>   refreshRate  { 30 };

    double getSampleRate() const { return currentSampleRate; }

private:
    double currentSampleRate = 48000.0;

    // --- FFT ---
    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t)fftSize,
        juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fftFifo {};
    std::array<float, fftSize * 2> fftWorkBuffer {};
    int fftFifoPos = 0;

    // --- K-weighting filters ---
    juce::dsp::IIR::Filter<float> kShelfL, kShelfR;
    juce::dsp::IIR::Filter<float> kHpfL, kHpfR;

    // --- LUFS ring buffer + running sums ---
    // Vectors: sized in prepareToPlay for any sample rate (44.1k–192k safe)
    std::vector<float> kwRingL, kwRingR;
    int kwRingSize = 0;
    int kwRingPos  = 0;
    int kwSamplesWritten = 0;

    double runningSumL_400ms = 0.0, runningSumR_400ms = 0.0;
    double runningSumL_3s    = 0.0, runningSumR_3s    = 0.0;
    int windowSamples400ms = 0;
    int windowSamples3s    = 0;

    // --- Gating blocks for integrated loudness ---
    static constexpr int kMaxGatingBlocks = 90000; // ~10 hours at 400ms blocks
    std::vector<float> gatingBlockLoudness;
    std::atomic<int> gatingBlockCount { 0 };
    int gatingAccumCounter = 0;
    double gatingAccumL = 0.0, gatingAccumR = 0.0;
    int gatingSamplesPerBlock = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReticleProcessor)
};
