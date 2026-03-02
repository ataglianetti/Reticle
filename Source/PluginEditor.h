#pragma once

#include "PluginProcessor.h"
#include "NeomorphicLookAndFeel.h"
#include "Meters/LevelMeter.h"
#include "Meters/SpectrumAnalyzer.h"
#include "Meters/LufsMeter.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

class ReticleEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReticleEditor(ReticleProcessor&);
    ~ReticleEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // Layout constants
    static constexpr int kHeaderH       = 44;
    static constexpr int kSettingsPanelW = 200;
    static constexpr int kModuleMinW    = 140;
    static constexpr int kDefaultH      = 500;

    ReticleProcessor& processor;
    NeomorphicLookAndFeel neoLnf;

    // Meter modules
    struct MeterSlot
    {
        juce::String name;
        std::unique_ptr<juce::Component> component;
        bool visible = true;
        int preferredWidth = 160; // hint, layout can override
    };
    std::vector<MeterSlot> meters;

    // Module toggle buttons (in header)
    juce::TextButton levelToggle  { "LVL" };
    juce::TextButton specToggle   { "SPEC" };
    juce::TextButton lufsToggle   { "LUFS" };
    juce::TextButton settingsToggle { "TUNE" };

    void setupToggle(juce::TextButton& btn, int meterIndex);
    void updateToggleAppearance(juce::TextButton& btn, bool active);
    void recalcWindowSize();

    // Settings panel
    bool settingsOpen = false;
    juce::Slider attackSlider, decaySlider, peakHoldSlider, dbFloorSlider, fpsSlider;
    juce::Label  attackLabel,  decayLabel,  peakHoldLabel,  dbFloorLabel,  fpsLabel;

    void setupSlider(juce::Slider&, juce::Label&, const juce::String& name,
                     double min, double max, double val, double step = 0.01);
    void drawSettingsPanel(juce::Graphics& g, juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReticleEditor)
};
