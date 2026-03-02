#pragma once

#include "../PluginProcessor.h"
#include "../NeomorphicLookAndFeel.h"

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter(ReticleProcessor& p) : processor(p)
    {
        startTimerHz(p.refreshRate.load());
    }

    ~LevelMeter() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        Neo::drawRaised(g, bounds, Neo::cornerRadius);

        auto inner = bounds.reduced(10.0f);

        // Module label
        g.setColour(Neo::textDim);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("LEVEL", inner.removeFromTop(16.0f), juce::Justification::centred, true);
        inner.removeFromTop(2.0f);

        float meterW = (inner.getWidth() - 24.0f) / 2.0f;
        auto leftBounds  = inner.removeFromLeft(meterW);
        auto scaleBounds = inner.removeFromLeft(24.0f);
        auto rightBounds = inner;

        drawBar(g, leftBounds,  smoothedPeakL, smoothedRmsL, peakHoldL, "L");
        drawScale(g, scaleBounds);
        drawBar(g, rightBounds, smoothedPeakR, smoothedRmsR, peakHoldR, "R");
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        float attack = processor.attackCoeff.load();
        float decay  = processor.decayCoeff.load();
        float holdSec = processor.peakHoldSec.load();
        int fps = processor.refreshRate.load();
        int holdFrames = (int)(holdSec * (float)fps);

        auto smooth = [&](float raw, float& s) {
            s = raw > s ? s + attack * (raw - s) : s * decay;
        };

        smooth(processor.peakLevelL.load(), smoothedPeakL);
        smooth(processor.peakLevelR.load(), smoothedPeakR);
        smooth(processor.rmsLevelL.load(),  smoothedRmsL);
        smooth(processor.rmsLevelR.load(),  smoothedRmsR);

        auto hold = [&](float peak, float& h, int& c) {
            if (peak >= h) { h = peak; c = holdFrames; }
            else if (c > 0) c--;
            else h *= decay;
        };

        hold(smoothedPeakL, peakHoldL, peakHoldCountL);
        hold(smoothedPeakR, peakHoldR, peakHoldCountR);
        repaint();
    }

    void drawBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                 float peak, float rms, float peakHold, const juce::String& label)
    {
        float floor = processor.dbFloor.load();

        g.setColour(Neo::text);
        g.setFont(juce::FontOptions(12.0f));
        auto labelArea = bounds.removeFromTop(16.0f);
        g.drawText(label, labelArea, juce::Justification::centred, true);
        bounds.removeFromTop(2.0f);

        float peakDb = juce::Decibels::gainToDecibels(peak, floor);
        auto readoutArea = bounds.removeFromBottom(16.0f);
        g.setColour(peakDb > -6.0f ? Neo::yellow : Neo::textDim);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(juce::String(peakDb, 1), readoutArea, juce::Justification::centred, true);
        bounds.removeFromBottom(2.0f);

        Neo::drawInset(g, bounds, 5.0f);
        auto m = bounds.reduced(3.0f);
        float h = m.getHeight();

        auto normDb = [&](float gain) {
            return juce::jmap(juce::Decibels::gainToDecibels(gain, floor), floor, 0.0f, 0.0f, 1.0f);
        };

        // RMS
        float rmsH = h * normDb(rms);
        if (rmsH > 0.5f)
        {
            auto rb = juce::Rectangle<float>(m.getX(), m.getBottom() - rmsH, m.getWidth(), rmsH);
            juce::ColourGradient grad(Neo::green, rb.getBottomLeft(), Neo::yellow, rb.getTopLeft(), false);
            if (normDb(rms) > 0.7f) grad.addColour(0.85, Neo::red.withAlpha(0.8f));
            g.setGradientFill(grad);
            g.fillRoundedRectangle(rb, 3.0f);
        }

        // Peak
        float peakH = h * normDb(peak);
        if (peakH > 0.5f)
        {
            float pw = juce::jmax(3.0f, m.getWidth() * 0.12f);
            auto pb = juce::Rectangle<float>(m.getCentreX() - pw * 0.5f, m.getBottom() - peakH, pw, peakH);
            auto c = normDb(peak) > 0.9f ? Neo::red : normDb(peak) > 0.7f ? Neo::yellow : Neo::green.brighter(0.3f);
            g.setColour(c.withAlpha(0.9f));
            g.fillRoundedRectangle(pb, 1.5f);
        }

        // Peak hold line
        float holdN = normDb(peakHold);
        if (holdN > 0.01f)
        {
            float hy = m.getBottom() - h * holdN;
            g.setColour((holdN > 0.9f ? Neo::red : holdN > 0.7f ? Neo::yellow : Neo::accent).withAlpha(0.85f));
            g.fillRect(m.getX() + 1.0f, hy - 1.0f, m.getWidth() - 2.0f, 2.0f);
        }
    }

    void drawScale(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float floor = processor.dbFloor.load();
        auto area = bounds.withTrimmedTop(18.0f).withTrimmedBottom(18.0f);
        float h = area.getHeight();
        g.setFont(juce::FontOptions(8.0f));

        for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
        {
            if (db < floor) continue;
            float norm = juce::jmap(db, floor, 0.0f, 0.0f, 1.0f);
            float y = area.getBottom() - h * norm;
            g.setColour(Neo::textDim.withAlpha(0.5f));
            g.fillRect(area.getX(), y, area.getWidth(), 0.5f);
            g.setColour(Neo::textDim);
            g.drawText(juce::String((int)db), (int)area.getX(), (int)(y - 5),
                       (int)area.getWidth(), 10, juce::Justification::centred, true);
        }
    }

    ReticleProcessor& processor;
    float smoothedPeakL = 0, smoothedPeakR = 0;
    float smoothedRmsL = 0, smoothedRmsR = 0;
    float peakHoldL = 0, peakHoldR = 0;
    int peakHoldCountL = 0, peakHoldCountR = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
