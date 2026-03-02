#include "PluginProcessor.h"
#include "PluginEditor.h"

ReticleProcessor::ReticleProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

ReticleProcessor::~ReticleProcessor() {}

void ReticleProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // Reset meters
    peakLevelL.store(0.0f);
    peakLevelR.store(0.0f);
    rmsLevelL.store(0.0f);
    rmsLevelR.store(0.0f);
}

void ReticleProcessor::releaseResources() {}

void ReticleProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0)
        return;

    // Left channel
    if (numChannels > 0)
    {
        peakLevelL.store(buffer.getMagnitude(0, 0, numSamples));
        rmsLevelL.store(buffer.getRMSLevel(0, 0, numSamples));
    }

    // Right channel
    if (numChannels > 1)
    {
        peakLevelR.store(buffer.getMagnitude(1, 0, numSamples));
        rmsLevelR.store(buffer.getRMSLevel(1, 0, numSamples));
    }

    // Pass-through: metering plugin does not modify audio
}

juce::AudioProcessorEditor* ReticleProcessor::createEditor()
{
    return new ReticleEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReticleProcessor();
}
