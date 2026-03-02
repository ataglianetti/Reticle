#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Neo
{
    // Dark neomorphic palette
    const juce::Colour bg        { 0xff2D2D44 };
    const juce::Colour bgLight   { 0xff363650 };
    const juce::Colour bgDark    { 0xff222236 };
    const juce::Colour surface   { 0xff33334D };
    const juce::Colour well      { 0xff1E1E32 };
    const juce::Colour text      { 0xffD0D0E0 };
    const juce::Colour textDim   { 0xff808098 };
    const juce::Colour accent    { 0xffE6A23C };
    const juce::Colour green     { 0xff4A9C6D };
    const juce::Colour yellow    { 0xffE6A23C };
    const juce::Colour red       { 0xffC45B4D };
    const juce::Colour shadowLt  { 0x10FFFFFF };
    const juce::Colour shadowDk  { 0x60000000 };

    const float cornerRadius = 10.0f;
    const float shadowOffset = 3.0f;

    inline void drawRaised(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = cornerRadius)
    {
        // Dark shadow (bottom-right)
        g.setColour(shadowDk);
        g.fillRoundedRectangle(bounds.translated(shadowOffset, shadowOffset), radius);
        // Light shadow (top-left)
        g.setColour(shadowLt);
        g.fillRoundedRectangle(bounds.translated(-shadowOffset * 0.5f, -shadowOffset * 0.5f), radius);
        // Surface with gradient
        juce::ColourGradient grad(bgLight, bounds.getTopLeft(), bgDark, bounds.getBottomRight(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, radius);
    }

    inline void drawInset(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = cornerRadius)
    {
        // Outer ring shadow (light top-left, dark bottom-right — inverted from raised)
        g.setColour(shadowDk);
        g.fillRoundedRectangle(bounds.expanded(1.0f), radius + 1.0f);
        g.setColour(shadowLt);
        g.fillRoundedRectangle(bounds.translated(1.5f, 1.5f).expanded(1.0f), radius + 1.0f);
        // Well fill
        g.setColour(well);
        g.fillRoundedRectangle(bounds, radius);
    }
}

class NeomorphicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeomorphicLookAndFeel()
    {
        setColour(juce::Slider::backgroundColourId, Neo::well);
        setColour(juce::Slider::trackColourId, Neo::accent);
        setColour(juce::Slider::thumbColourId, Neo::bgLight);
        setColour(juce::Label::textColourId, Neo::text);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minPos*/, float /*maxPos*/,
                          juce::Slider::SliderStyle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        float trackH = 4.0f;
        auto trackBounds = bounds.withSizeKeepingCentre(bounds.getWidth() - 10.0f, trackH);

        // Track well
        Neo::drawInset(g, trackBounds, 2.0f);

        // Filled portion
        float fillWidth = sliderPos - trackBounds.getX();
        if (fillWidth > 0)
        {
            auto fillBounds = trackBounds.withWidth(fillWidth);
            juce::ColourGradient grad(Neo::accent.brighter(0.2f), fillBounds.getTopLeft(),
                                      Neo::accent.darker(0.1f), fillBounds.getBottomRight(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillBounds, 2.0f);
        }

        // Thumb — raised circle
        float thumbSize = 16.0f;
        auto thumbBounds = juce::Rectangle<float>(thumbSize, thumbSize)
                               .withCentre({ sliderPos, trackBounds.getCentreY() });
        Neo::drawRaised(g, thumbBounds, thumbSize * 0.5f);

        // Thumb inner dot
        g.setColour(Neo::accent);
        g.fillEllipse(thumbBounds.reduced(4.0f));
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(label.getFont());
        g.drawText(label.getText(), label.getLocalBounds(),
                   label.getJustificationType(), true);
    }
};
