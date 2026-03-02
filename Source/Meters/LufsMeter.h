#pragma once

#include "../PluginProcessor.h"
#include "../NeomorphicLookAndFeel.h"

class LufsMeter : public juce::Component, private juce::Timer
{
public:
    explicit LufsMeter(ReticleProcessor& p) : processor(p)
    {
        startTimerHz(p.refreshRate.load());
    }

    ~LufsMeter() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        Neo::drawRaised(g, bounds, Neo::cornerRadius);

        auto inner = bounds.reduced(10.0f);

        // Module label
        g.setColour(Neo::textDim);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("LOUDNESS", inner.removeFromTop(16.0f), juce::Justification::centred, true);
        inner.removeFromTop(4.0f);

        // Big integrated readout
        drawBigReadout(g, inner.removeFromTop(50.0f));
        inner.removeFromTop(6.0f);

        // Momentary and short-term bars
        float barAreaH = (inner.getHeight() - 8.0f) / 2.0f;
        drawLufsBar(g, inner.removeFromTop(barAreaH), "MOMENTARY",
                    smoothedMomentary, Neo::green);
        inner.removeFromTop(8.0f);
        drawLufsBar(g, inner, "SHORT TERM",
                    smoothedShortTerm, Neo::accent);
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        float m = processor.lufsMomentary.load();
        float s = processor.lufsShortTerm.load();
        float i = processor.computeGatedIntegrated(); // computed on UI thread

        // Smooth with fast attack, slow release
        auto smooth = [](float raw, float& val) {
            if (raw > val)
                val = val + 0.3f * (raw - val);
            else
                val = val + 0.05f * (raw - val);
        };

        smooth(m, smoothedMomentary);
        smooth(s, smoothedShortTerm);
        smooth(i, smoothedIntegrated);

        repaint();
    }

    void drawBigReadout(juce::Graphics& g, juce::Rectangle<float> area)
    {
        Neo::drawInset(g, area, 8.0f);
        auto inner = area.reduced(6.0f);

        // Integrated LUFS value — big number
        juce::String valueStr;
        if (smoothedIntegrated <= -100.0f)
            valueStr = "---";
        else
            valueStr = juce::String(smoothedIntegrated, 1);

        g.setColour(Neo::text);
        g.setFont(juce::FontOptions(28.0f));
        g.drawText(valueStr, inner, juce::Justification::centred, true);

        // Unit label
        g.setColour(Neo::textDim);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText("LUFS INTEGRATED", inner.withTrimmedTop(inner.getHeight() - 12.0f),
                   juce::Justification::centred, true);
    }

    void drawLufsBar(juce::Graphics& g, juce::Rectangle<float> area,
                     const juce::String& label, float value, juce::Colour colour)
    {
        // Label
        auto labelArea = area.removeFromTop(12.0f);
        g.setColour(Neo::textDim);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(label, labelArea, juce::Justification::centredLeft, true);

        // Value readout
        juce::String valStr = value <= -100.0f ? "---" : juce::String(value, 1) + " LUFS";
        g.drawText(valStr, labelArea, juce::Justification::centredRight, true);
        area.removeFromTop(2.0f);

        // Bar well
        auto barArea = area.withHeight(juce::jmin(area.getHeight(), 12.0f));
        Neo::drawInset(g, barArea, 3.0f);

        // Fill
        // Map LUFS to bar: -60 LUFS = 0%, 0 LUFS = 100%
        float norm = juce::jmap(juce::jlimit(-60.0f, 0.0f, value), -60.0f, 0.0f, 0.0f, 1.0f);
        if (norm > 0.001f)
        {
            auto fillArea = barArea.reduced(2.0f).withWidth(barArea.reduced(2.0f).getWidth() * norm);
            juce::ColourGradient grad(colour.withAlpha(0.6f), fillArea.getTopLeft(),
                                      colour, fillArea.getTopRight(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillArea, 2.0f);
        }

        // Target markers at common loudness standards
        auto markerArea = barArea.reduced(2.0f);
        g.setColour(Neo::textDim.withAlpha(0.4f));
        for (float target : { -14.0f, -16.0f, -23.0f }) // Spotify, Apple, broadcast
        {
            float xNorm = juce::jmap(target, -60.0f, 0.0f, 0.0f, 1.0f);
            float x = markerArea.getX() + markerArea.getWidth() * xNorm;
            g.drawVerticalLine((int)x, markerArea.getY(), markerArea.getBottom());
        }
    }

    ReticleProcessor& processor;
    float smoothedMomentary  = -100.0f;
    float smoothedShortTerm  = -100.0f;
    float smoothedIntegrated = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LufsMeter)
};
