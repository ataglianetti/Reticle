#pragma once

#include "PluginProcessor.h"
#include "NeomorphicLookAndFeel.h"
#include <juce_gui_extra/juce_gui_extra.h>

class ReticleEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit ReticleEditor(ReticleProcessor&);
    ~ReticleEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // Drawing
    void drawMeterModule(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawMeterBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                      float peak, float rms, float peakHold, const juce::String& label);
    void drawDbScale(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSettingsPanel(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Layout
    static constexpr int kHeaderH       = 44;
    static constexpr int kSettingsPanelW = 200;
    static constexpr int kMeterMinW     = 120;
    static constexpr int kDefaultW      = 360;
    static constexpr int kDefaultH      = 480;

    ReticleProcessor& processor;
    NeomorphicLookAndFeel neoLnf;

    // Smoothed meter values
    float smoothedPeakL = 0.0f, smoothedPeakR = 0.0f;
    float smoothedRmsL  = 0.0f, smoothedRmsR  = 0.0f;
    float peakHoldL     = 0.0f, peakHoldR     = 0.0f;
    int   peakHoldCountL = 0,   peakHoldCountR = 0;

    // Settings panel
    bool settingsOpen = false;
    juce::TextButton settingsToggle;

    juce::Slider attackSlider, decaySlider, peakHoldSlider, dbFloorSlider, fpsSlider;
    juce::Label  attackLabel,  decayLabel,  peakHoldLabel,  dbFloorLabel,  fpsLabel;

    void setupSlider(juce::Slider&, juce::Label&, const juce::String& name,
                     double min, double max, double val, double step = 0.01);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReticleEditor)
};
