#include "PluginEditor.h"

ReticleEditor::ReticleEditor(ReticleProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&neoLnf);

    // Create meter modules
    meters.push_back({ "Level",    std::make_unique<LevelMeter>(p),       true, 140 });
    meters.push_back({ "Spectrum", std::make_unique<SpectrumAnalyzer>(p), true, 280 });
    meters.push_back({ "LUFS",    std::make_unique<LufsMeter>(p),        true, 180 });

    for (auto& slot : meters)
        addAndMakeVisible(slot.component.get());

    // Module toggles
    setupToggle(levelToggle, 0);
    setupToggle(specToggle,  1);
    setupToggle(lufsToggle,  2);

    // Settings toggle
    settingsToggle.setColour(juce::TextButton::buttonColourId, Neo::surface);
    settingsToggle.setColour(juce::TextButton::textColourOffId, Neo::textDim);
    settingsToggle.onClick = [this]
    {
        settingsOpen = !settingsOpen;
        updateToggleAppearance(settingsToggle, settingsOpen);
        recalcWindowSize();
    };
    addAndMakeVisible(settingsToggle);

    // Settings sliders
    setupSlider(attackSlider,   attackLabel,   "Attack",     0.01, 1.0,   p.attackCoeff.load(),  0.01);
    setupSlider(decaySlider,    decayLabel,    "Decay",      0.80, 0.999, p.decayCoeff.load(),   0.001);
    setupSlider(peakHoldSlider, peakHoldLabel, "Peak Hold",  0.0,  5.0,   p.peakHoldSec.load(),  0.1);
    setupSlider(dbFloorSlider,  dbFloorLabel,  "dB Floor",  -96.0, -24.0, p.dbFloor.load(),      1.0);
    setupSlider(fpsSlider,      fpsLabel,      "FPS",        15.0, 60.0,  (double)p.refreshRate.load(), 1.0);

    attackSlider.onValueChange   = [this] { processor.attackCoeff.store((float)attackSlider.getValue()); };
    decaySlider.onValueChange    = [this] { processor.decayCoeff.store((float)decaySlider.getValue()); };
    peakHoldSlider.onValueChange = [this] { processor.peakHoldSec.store((float)peakHoldSlider.getValue()); };
    dbFloorSlider.onValueChange  = [this] { processor.dbFloor.store((float)dbFloorSlider.getValue()); };
    fpsSlider.onValueChange      = [this] { processor.refreshRate.store((int)fpsSlider.getValue()); };

    // Initial size
    recalcWindowSize();
    setResizable(true, true);
    setResizeLimits(200, 300, 1600, 1000);
}

ReticleEditor::~ReticleEditor()
{
    setLookAndFeel(nullptr);
}

void ReticleEditor::setupToggle(juce::TextButton& btn, int meterIndex)
{
    btn.setColour(juce::TextButton::buttonColourId, Neo::accent.withAlpha(0.3f));
    btn.setColour(juce::TextButton::textColourOffId, Neo::text);
    btn.onClick = [this, meterIndex, &btn]
    {
        auto& slot = meters[(size_t)meterIndex];
        slot.visible = !slot.visible;
        slot.component->setVisible(slot.visible);
        updateToggleAppearance(btn, slot.visible);
        recalcWindowSize();
    };
    addAndMakeVisible(btn);
    updateToggleAppearance(btn, meters[(size_t)meterIndex].visible);
}

void ReticleEditor::updateToggleAppearance(juce::TextButton& btn, bool active)
{
    btn.setColour(juce::TextButton::buttonColourId,
                  active ? Neo::accent.withAlpha(0.3f) : Neo::surface);
    btn.setColour(juce::TextButton::textColourOffId,
                  active ? Neo::text : Neo::textDim.withAlpha(0.5f));
}

void ReticleEditor::recalcWindowSize()
{
    int totalW = 0;
    for (auto& slot : meters)
    {
        if (slot.visible)
            totalW += slot.preferredWidth;
    }

    if (totalW == 0) totalW = 200; // minimum if all hidden
    if (settingsOpen) totalW += kSettingsPanelW;

    totalW = juce::jmax(280, totalW + 16); // padding
    setSize(totalW, kDefaultH);
}

void ReticleEditor::setupSlider(juce::Slider& slider, juce::Label& label,
                                const juce::String& name, double min, double max,
                                double val, double step)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setRange(min, max, step);
    slider.setValue(val, juce::dontSendNotification);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    slider.setColour(juce::Slider::textBoxTextColourId, Neo::text);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, Neo::well);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addChildComponent(slider);

    label.setText(name, juce::dontSendNotification);
    label.setFont(juce::FontOptions(12.0f));
    label.setColour(juce::Label::textColourId, Neo::textDim);
    label.setJustificationType(juce::Justification::centredLeft);
    addChildComponent(label);
}

void ReticleEditor::paint(juce::Graphics& g)
{
    g.fillAll(Neo::bg);

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(kHeaderH).toFloat();
    {
        juce::ColourGradient grad(Neo::bgLight, headerBounds.getTopLeft(),
                                  Neo::bg, headerBounds.getBottomLeft(), false);
        g.setGradientFill(grad);
        g.fillRect(headerBounds);

        g.setColour(Neo::text);
        g.setFont(juce::FontOptions(15.0f));
        g.drawText("RETICLE", headerBounds.reduced(14.0f, 0).removeFromLeft(80.0f),
                   juce::Justification::centredLeft, true);

        // Separator
        g.setColour(Neo::shadowDk);
        g.fillRect(headerBounds.getX(), headerBounds.getBottom() - 1.0f,
                   headerBounds.getWidth(), 1.0f);
    }

    // Settings panel background
    if (settingsOpen)
    {
        auto body = getLocalBounds().withTrimmedTop(kHeaderH).toFloat();
        auto settingsBounds = body.removeFromRight((float)kSettingsPanelW);
        drawSettingsPanel(g, settingsBounds);
    }
}

void ReticleEditor::drawSettingsPanel(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(Neo::shadowDk);
    g.fillRect(bounds.getX(), bounds.getY(), 1.0f, bounds.getHeight());

    g.setColour(Neo::bgDark);
    g.fillRect(bounds.withTrimmedLeft(1.0f));

    g.setColour(Neo::textDim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("TUNING", bounds.removeFromTop(28.0f).reduced(12.0f, 0),
               juce::Justification::centredLeft, true);
}

void ReticleEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(kHeaderH);

    // Header buttons — right side
    auto btnArea = header.reduced(0, 8).withTrimmedLeft(100);
    int btnW = 48;
    int btnGap = 4;

    // Right-align: TUNE button at far right
    settingsToggle.setBounds(btnArea.removeFromRight(btnW).reduced(0, 0));
    btnArea.removeFromRight(btnGap + 8); // extra gap before TUNE

    // Module toggles
    lufsToggle.setBounds(btnArea.removeFromRight(btnW));
    btnArea.removeFromRight(btnGap);
    specToggle.setBounds(btnArea.removeFromRight(btnW));
    btnArea.removeFromRight(btnGap);
    levelToggle.setBounds(btnArea.removeFromRight(btnW));

    // Body area
    auto body = area;

    // Settings panel sliders
    if (settingsOpen)
    {
        auto panel = body.removeFromRight(kSettingsPanelW);
        panel.removeFromTop(30);
        panel = panel.reduced(12, 0);

        struct SliderRow { juce::Slider& s; juce::Label& l; };
        SliderRow rows[] = {
            { attackSlider, attackLabel }, { decaySlider, decayLabel },
            { peakHoldSlider, peakHoldLabel }, { dbFloorSlider, dbFloorLabel },
            { fpsSlider, fpsLabel }
        };
        for (auto& row : rows)
        {
            row.l.setBounds(panel.removeFromTop(16));
            row.l.setVisible(true);
            row.s.setBounds(panel.removeFromTop(28));
            row.s.setVisible(true);
            panel.removeFromTop(8);
        }
    }
    else
    {
        attackSlider.setVisible(false);   attackLabel.setVisible(false);
        decaySlider.setVisible(false);    decayLabel.setVisible(false);
        peakHoldSlider.setVisible(false); peakHoldLabel.setVisible(false);
        dbFloorSlider.setVisible(false);  dbFloorLabel.setVisible(false);
        fpsSlider.setVisible(false);      fpsLabel.setVisible(false);
    }

    // Layout visible meters into body
    int visibleCount = 0;
    int totalPreferred = 0;
    for (auto& slot : meters)
    {
        if (slot.visible) { visibleCount++; totalPreferred += slot.preferredWidth; }
    }

    if (visibleCount > 0)
    {
        float availW = (float)body.getWidth();
        for (auto& slot : meters)
        {
            if (!slot.visible)
            {
                slot.component->setBounds(0, 0, 0, 0);
                continue;
            }

            // Distribute proportionally based on preferred widths
            float proportion = (float)slot.preferredWidth / (float)totalPreferred;
            int w = juce::jmax(kModuleMinW, (int)(availW * proportion));
            auto moduleBounds = body.removeFromLeft(w);
            slot.component->setBounds(moduleBounds);
        }
    }
}
