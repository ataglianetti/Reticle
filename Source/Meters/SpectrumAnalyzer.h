#pragma once

#include "../PluginProcessor.h"
#include "../NeomorphicLookAndFeel.h"

class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer(ReticleProcessor& p) : processor(p)
    {
        smoothedSpectrum.fill(0.0f);
        startTimerHz(p.refreshRate.load());
    }

    ~SpectrumAnalyzer() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        Neo::drawRaised(g, bounds, Neo::cornerRadius);

        auto inner = bounds.reduced(10.0f);

        // Module label
        g.setColour(Neo::textDim);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("SPECTRUM", inner.removeFromTop(16.0f), juce::Justification::centred, true);
        inner.removeFromTop(2.0f);

        // Frequency labels at bottom
        auto freqLabelArea = inner.removeFromBottom(14.0f);
        drawFreqLabels(g, freqLabelArea);
        inner.removeFromBottom(2.0f);

        // dB labels on left
        auto dbLabelArea = inner.removeFromLeft(22.0f);
        inner.removeFromLeft(2.0f);

        // Spectrum well
        Neo::drawInset(g, inner, 5.0f);
        auto plotArea = inner.reduced(3.0f);

        drawDbLabels(g, dbLabelArea, plotArea);
        drawSpectrum(g, plotArea);
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        if (processor.spectrumReady.exchange(false))
        {
            int readBank = processor.spectrumWriteBank.load(); // read the latest completed bank
            float decay = 0.7f;
            for (int i = 0; i < ReticleProcessor::fftBins; ++i)
            {
                float mag = processor.spectrumBanks[readBank][(size_t)i];
                float db = juce::Decibels::gainToDecibels(mag, -90.0f);
                float norm = juce::jmap(db, -90.0f, 0.0f, 0.0f, 1.0f);
                norm = juce::jlimit(0.0f, 1.0f, norm);

                if (norm > smoothedSpectrum[(size_t)i])
                    smoothedSpectrum[(size_t)i] = norm;
                else
                    smoothedSpectrum[(size_t)i] = smoothedSpectrum[(size_t)i] * decay + norm * (1.0f - decay);
            }
        }
        repaint();
    }

    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (area.getWidth() < 2.0f || area.getHeight() < 2.0f) return;

        double sr = processor.getSampleRate();
        float w = area.getWidth();
        float h = area.getHeight();

        // Build path using log frequency mapping
        juce::Path specPath;
        juce::Path fillPath;
        bool started = false;

        float minFreq = 20.0f;
        float maxFreq = (float)(sr * 0.5);
        float logMin = std::log10(minFreq);
        float logMax = std::log10(maxFreq);

        for (int px = 0; px < (int)w; ++px)
        {
            // Map pixel to frequency (log scale)
            float logFreq = logMin + ((float)px / w) * (logMax - logMin);
            float freq = std::pow(10.0f, logFreq);

            // Map frequency to FFT bin
            float binFloat = freq / (float)(sr / (double)ReticleProcessor::fftSize);
            int bin = juce::jlimit(0, ReticleProcessor::fftBins - 1, (int)binFloat);

            // Interpolate between bins for smoother curve
            float frac = binFloat - (float)bin;
            int nextBin = juce::jmin(bin + 1, ReticleProcessor::fftBins - 1);
            float val = smoothedSpectrum[(size_t)bin] * (1.0f - frac)
                      + smoothedSpectrum[(size_t)nextBin] * frac;

            float x = area.getX() + (float)px;
            float y = area.getBottom() - val * h;

            if (!started)
            {
                specPath.startNewSubPath(x, y);
                fillPath.startNewSubPath(area.getX(), area.getBottom());
                fillPath.lineTo(x, y);
                started = true;
            }
            else
            {
                specPath.lineTo(x, y);
                fillPath.lineTo(x, y);
            }
        }

        // Fill gradient under curve
        fillPath.lineTo(area.getRight(), area.getBottom());
        fillPath.closeSubPath();

        juce::ColourGradient fillGrad(Neo::accent.withAlpha(0.25f), area.getX(), area.getBottom(),
                                      Neo::green.withAlpha(0.08f), area.getX(), area.getY(), false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);

        // Stroke line
        juce::ColourGradient lineGrad(Neo::green, area.getBottomLeft(),
                                      Neo::accent, area.getTopRight(), false);
        g.setGradientFill(lineGrad);
        g.strokePath(specPath, juce::PathStrokeType(1.5f));

        // Grid lines (horizontal dB)
        g.setColour(Neo::textDim.withAlpha(0.12f));
        for (float db : { -12.0f, -24.0f, -48.0f, -72.0f })
        {
            float norm = juce::jmap(db, -90.0f, 0.0f, 0.0f, 1.0f);
            float gy = area.getBottom() - norm * h;
            g.drawHorizontalLine((int)gy, area.getX(), area.getRight());
        }

        // Vertical frequency grid
        for (float freq : { 100.0f, 1000.0f, 10000.0f })
        {
            float logF = std::log10(freq);
            float xNorm = (logF - logMin) / (logMax - logMin);
            float gx = area.getX() + xNorm * w;
            g.drawVerticalLine((int)gx, area.getY(), area.getBottom());
        }
    }

    void drawFreqLabels(juce::Graphics& g, juce::Rectangle<float> area)
    {
        double sr = processor.getSampleRate();
        float logMin = std::log10(20.0f);
        float logMax = std::log10((float)(sr * 0.5));
        float w = area.getWidth();

        g.setColour(Neo::textDim);
        g.setFont(juce::FontOptions(8.0f));

        for (auto [freq, label] : std::initializer_list<std::pair<float, const char*>>{
            {100.0f, "100"}, {1000.0f, "1k"}, {10000.0f, "10k"}})
        {
            float xNorm = (std::log10(freq) - logMin) / (logMax - logMin);
            float x = area.getX() + xNorm * w;
            g.drawText(label, (int)(x - 14), (int)area.getY(), 28, (int)area.getHeight(),
                       juce::Justification::centred, true);
        }
    }

    void drawDbLabels(juce::Graphics& g, juce::Rectangle<float> labelArea,
                      juce::Rectangle<float> plotArea)
    {
        g.setColour(Neo::textDim);
        g.setFont(juce::FontOptions(8.0f));
        float h = plotArea.getHeight();

        for (auto [db, label] : std::initializer_list<std::pair<float, const char*>>{
            {0.0f, "0"}, {-24.0f, "-24"}, {-48.0f, "-48"}, {-72.0f, "-72"}})
        {
            float norm = juce::jmap(db, -90.0f, 0.0f, 0.0f, 1.0f);
            float y = plotArea.getBottom() - norm * h;
            g.drawText(label, (int)labelArea.getX(), (int)(y - 5),
                       (int)labelArea.getWidth(), 10, juce::Justification::centredRight, true);
        }
    }

    ReticleProcessor& processor;
    std::array<float, ReticleProcessor::fftBins> smoothedSpectrum {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
