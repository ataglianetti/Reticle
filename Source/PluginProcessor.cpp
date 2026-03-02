#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

ReticleProcessor::ReticleProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

ReticleProcessor::~ReticleProcessor() {}

void ReticleProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Level meters
    peakLevelL.store(0.0f);  peakLevelR.store(0.0f);
    rmsLevelL.store(0.0f);   rmsLevelR.store(0.0f);

    // FFT
    fftFifoPos = 0;
    fftFifo.fill(0.0f);
    spectrumBanks[0].fill(0.0f);
    spectrumBanks[1].fill(0.0f);

    // K-weighting filters
    auto shelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, 1500.0, 0.7071f, 3.9997f);
    auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate, 38.0, 0.5f);

    kShelfL.coefficients = shelfCoeffs;  kShelfR.coefficients = shelfCoeffs;
    kHpfL.coefficients   = hpfCoeffs;   kHpfR.coefficients   = hpfCoeffs;
    kShelfL.reset(); kShelfR.reset();
    kHpfL.reset();   kHpfR.reset();

    // LUFS ring buffer — sized for actual sample rate (safe up to 192kHz)
    windowSamples400ms = (int)(sampleRate * 0.4);
    windowSamples3s    = (int)(sampleRate * 3.0);
    kwRingSize = windowSamples3s + 1;
    kwRingPos = 0;
    kwSamplesWritten = 0;

    kwRingL.assign((size_t)kwRingSize, 0.0f);
    kwRingR.assign((size_t)kwRingSize, 0.0f);

    runningSumL_400ms = 0.0; runningSumR_400ms = 0.0;
    runningSumL_3s    = 0.0; runningSumR_3s    = 0.0;

    // Gating blocks — pre-allocate once
    gatingBlockLoudness.resize((size_t)kMaxGatingBlocks, 0.0f);
    resetIntegratedLoudness();
    gatingSamplesPerBlock = (int)(sampleRate * 0.4);
}

void ReticleProcessor::releaseResources() {}

void ReticleProcessor::resetIntegratedLoudness()
{
    gatingBlockCount.store(0);
    gatingAccumCounter = 0;
    gatingAccumL = 0.0;
    gatingAccumR = 0.0;
}

float ReticleProcessor::computeGatedIntegrated() const
{
    int count = gatingBlockCount.load();
    if (count == 0) return -100.0f;

    double sumAbove70 = 0.0;
    int countAbove70 = 0;

    for (int i = 0; i < count; ++i)
    {
        float bl = gatingBlockLoudness[(size_t)i];
        if (bl > -70.0f)
        {
            sumAbove70 += std::pow(10.0, (double)bl / 10.0);
            countAbove70++;
        }
    }
    if (countAbove70 == 0) return -100.0f;

    double gammaA = 10.0 * std::log10(sumAbove70 / (double)countAbove70);
    double relThreshold = gammaA - 10.0;

    double sumGated = 0.0;
    int countGated = 0;

    for (int i = 0; i < count; ++i)
    {
        float bl = gatingBlockLoudness[(size_t)i];
        if (bl > -70.0f && (double)bl > relThreshold)
        {
            sumGated += std::pow(10.0, (double)bl / 10.0);
            countGated++;
        }
    }
    if (countGated == 0) return -100.0f;

    return (float)(-0.691 + 10.0 * std::log10(sumGated / (double)countGated));
}

void ReticleProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    if (numSamples == 0) return;

    // --- 1. Level metering (JUCE block ops) ---
    if (numChannels > 0)
    {
        peakLevelL.store(buffer.getMagnitude(0, 0, numSamples));
        rmsLevelL.store(buffer.getRMSLevel(0, 0, numSamples));
    }
    if (numChannels > 1)
    {
        peakLevelR.store(buffer.getMagnitude(1, 0, numSamples));
        rmsLevelR.store(buffer.getRMSLevel(1, 0, numSamples));
    }

    // Guard: ring buffers must be allocated
    if (kwRingSize == 0 || kwRingL.empty()) return;

    const float* inL = numChannels > 0 ? buffer.getReadPointer(0) : nullptr;
    const float* inR = numChannels > 1 ? buffer.getReadPointer(1) : nullptr;

    // --- 2. Per-sample: FFT + K-weighting + LUFS running sums ---
    for (int i = 0; i < numSamples; ++i)
    {
        float sL = inL ? inL[i] : 0.0f;
        float sR = inR ? inR[i] : sL;

        // FFT FIFO
        fftFifo[(size_t)fftFifoPos] = (sL + sR) * 0.5f;
        if (++fftFifoPos >= fftSize)
        {
            fftFifoPos = 0;
            int writeBank = 1 - spectrumWriteBank.load();
            std::copy(fftFifo.begin(), fftFifo.end(), fftWorkBuffer.begin());
            std::fill(fftWorkBuffer.begin() + fftSize, fftWorkBuffer.end(), 0.0f);
            window.multiplyWithWindowingTable(fftWorkBuffer.data(), (size_t)fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform(fftWorkBuffer.data());
            for (int b = 0; b < fftBins; ++b)
                spectrumBanks[writeBank][(size_t)b] = fftWorkBuffer[(size_t)b];
            spectrumWriteBank.store(writeBank);
            spectrumReady.store(true);
        }

        // K-weighting
        float kwL = kHpfL.processSample(kShelfL.processSample(sL));
        float kwR = kHpfR.processSample(kShelfR.processSample(sR));
        float sqL = kwL * kwL;
        float sqR = kwR * kwR;

        // Running sum: subtract sample leaving 400ms window
        if (kwSamplesWritten >= windowSamples400ms)
        {
            int oldPos = (kwRingPos - windowSamples400ms + kwRingSize) % kwRingSize;
            runningSumL_400ms -= (double)kwRingL[(size_t)oldPos];
            runningSumR_400ms -= (double)kwRingR[(size_t)oldPos];
        }

        // Running sum: subtract sample leaving 3s window
        if (kwSamplesWritten >= windowSamples3s)
        {
            int oldPos = (kwRingPos - windowSamples3s + kwRingSize) % kwRingSize;
            runningSumL_3s -= (double)kwRingL[(size_t)oldPos];
            runningSumR_3s -= (double)kwRingR[(size_t)oldPos];
        }

        // Write to ring buffer + add to running sums
        kwRingL[(size_t)kwRingPos] = sqL;
        kwRingR[(size_t)kwRingPos] = sqR;
        runningSumL_400ms += (double)sqL;  runningSumR_400ms += (double)sqR;
        runningSumL_3s    += (double)sqL;  runningSumR_3s    += (double)sqR;

        kwRingPos = (kwRingPos + 1) % kwRingSize;
        kwSamplesWritten++;

        // Gating block accumulation
        gatingAccumL += (double)sqL;
        gatingAccumR += (double)sqR;
        if (++gatingAccumCounter >= gatingSamplesPerBlock)
        {
            int idx = gatingBlockCount.load();
            if (idx < kMaxGatingBlocks)
            {
                double meanL = gatingAccumL / (double)gatingSamplesPerBlock;
                double meanR = gatingAccumR / (double)gatingSamplesPerBlock;
                gatingBlockLoudness[(size_t)idx] =
                    (float)(-0.691 + 10.0 * std::log10(meanL + meanR + 1e-20));
                gatingBlockCount.store(idx + 1);
            }
            gatingAccumCounter = 0;
            gatingAccumL = 0.0;
            gatingAccumR = 0.0;
        }
    }

    // --- 3. LUFS from running sums (O(1)) ---
    int mSamples = juce::jmin(kwSamplesWritten, windowSamples400ms);
    int sSamples = juce::jmin(kwSamplesWritten, windowSamples3s);

    if (mSamples > 0)
    {
        double ms = juce::jmax(0.0, runningSumL_400ms + runningSumR_400ms) / (double)mSamples;
        lufsMomentary.store((float)(-0.691 + 10.0 * std::log10(ms + 1e-20)));
    }
    if (sSamples > 0)
    {
        double ms = juce::jmax(0.0, runningSumL_3s + runningSumR_3s) / (double)sSamples;
        lufsShortTerm.store((float)(-0.691 + 10.0 * std::log10(ms + 1e-20)));
    }
}

juce::AudioProcessorEditor* ReticleProcessor::createEditor()
{
    return new ReticleEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReticleProcessor();
}
