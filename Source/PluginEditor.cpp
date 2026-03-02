#include "PluginEditor.h"

ReticleEditor::ReticleEditor(ReticleProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&neoLnf);
    setSize(kDefaultW, kDefaultH);
    setResizable(true, true);
    setResizeLimits(280, 360, 1200, 900);

    // Settings toggle button
    settingsToggle.setButtonText("TUNE");
    settingsToggle.setColour(juce::TextButton::buttonColourId, Neo::surface);
    settingsToggle.setColour(juce::TextButton::textColourOffId, Neo::textDim);
    settingsToggle.onClick = [this]
    {
        settingsOpen = !settingsOpen;
        int newW = settingsOpen ? getWidth() + kSettingsPanelW : getWidth() - kSettingsPanelW;
        setSize(juce::jmax(280, newW), getHeight());
        resized();
        repaint();
    };
    addAndMakeVisible(settingsToggle);

    // Tunable sliders
    setupSlider(attackSlider,   attackLabel,   "Attack",     0.01, 1.0,  p.attackCoeff.load(),  0.01);
    setupSlider(decaySlider,    decayLabel,    "Decay",      0.80, 0.999, p.decayCoeff.load(),  0.001);
    setupSlider(peakHoldSlider, peakHoldLabel, "Peak Hold",  0.0,  5.0,  p.peakHoldSec.load(),  0.1);
    setupSlider(dbFloorSlider,  dbFloorLabel,  "dB Floor",  -96.0, -24.0, p.dbFloor.load(),     1.0);
    setupSlider(fpsSlider,      fpsLabel,      "FPS",        15.0, 60.0, (double)p.refreshRate.load(), 1.0);

    // Sync slider changes back to processor
    attackSlider.onValueChange   = [this] { processor.attackCoeff.store((float)attackSlider.getValue()); };
    decaySlider.onValueChange    = [this] { processor.decayCoeff.store((float)decaySlider.getValue()); };
    peakHoldSlider.onValueChange = [this] { processor.peakHoldSec.store((float)peakHoldSlider.getValue()); };
    dbFloorSlider.onValueChange  = [this] { processor.dbFloor.store((float)dbFloorSlider.getValue()); repaint(); };
    fpsSlider.onValueChange      = [this]
    {
        int fps = (int)fpsSlider.getValue();
        processor.refreshRate.store(fps);
        stopTimer();
        startTimerHz(fps);
    };

    startTimerHz(p.refreshRate.load());
}

ReticleEditor::~ReticleEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
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

void ReticleEditor::timerCallback()
{
    float attack = processor.attackCoeff.load();
    float decay  = processor.decayCoeff.load();
    float holdSec = processor.peakHoldSec.load();
    int fps = processor.refreshRate.load();
    int holdFrames = (int)(holdSec * (float)fps);

    float peakL = processor.peakLevelL.load();
    float peakR = processor.peakLevelR.load();
    float rmsL  = processor.rmsLevelL.load();
    float rmsR  = processor.rmsLevelR.load();

    // Smoothing: fast attack, configurable decay
    auto smooth = [&](float raw, float& smoothed)
    {
        if (raw > smoothed)
            smoothed = smoothed + attack * (raw - smoothed);
        else
            smoothed = smoothed * decay;
    };

    smooth(peakL, smoothedPeakL);
    smooth(peakR, smoothedPeakR);
    smooth(rmsL,  smoothedRmsL);
    smooth(rmsR,  smoothedRmsR);

    // Peak hold
    auto updateHold = [&](float peak, float& hold, int& counter)
    {
        if (peak >= hold)
        {
            hold = peak;
            counter = holdFrames;
        }
        else if (counter > 0)
            counter--;
        else
            hold = hold * decay;
    };

    updateHold(smoothedPeakL, peakHoldL, peakHoldCountL);
    updateHold(smoothedPeakR, peakHoldR, peakHoldCountR);

    repaint();
}

// ---- Drawing ----

void ReticleEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(Neo::bg);

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(kHeaderH).toFloat();
    {
        juce::ColourGradient grad(Neo::bgLight, headerBounds.getTopLeft(),
                                  Neo::bg, headerBounds.getBottomLeft(), false);
        g.setGradientFill(grad);
        g.fillRect(headerBounds);

        g.setColour(Neo::text);
        g.setFont(juce::FontOptions(16.0f));
        g.drawText("RETICLE", headerBounds.reduced(16.0f, 0), juce::Justification::centredLeft, true);

        // Subtle separator
        g.setColour(Neo::shadowDk);
        g.fillRect(headerBounds.getX(), headerBounds.getBottom() - 1.0f,
                   headerBounds.getWidth(), 1.0f);
    }

    // Meter module area
    auto body = getLocalBounds().withTrimmedTop(kHeaderH).toFloat();

    if (settingsOpen)
    {
        auto settingsBounds = body.removeFromRight((float)kSettingsPanelW);
        drawSettingsPanel(g, settingsBounds);
    }

    drawMeterModule(g, body);
}

void ReticleEditor::drawMeterModule(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto bounds = area.reduced(12.0f);

    // Raised panel
    Neo::drawRaised(g, bounds, Neo::cornerRadius);

    auto inner = bounds.reduced(14.0f);
    float meterW = (inner.getWidth() - 30.0f) / 2.0f; // two meters + scale in middle

    auto leftBounds  = inner.removeFromLeft(meterW);
    auto scaleBounds = inner.removeFromLeft(30.0f);
    auto rightBounds = inner;

    drawMeterBar(g, leftBounds,  smoothedPeakL, smoothedRmsL, peakHoldL, "L");
    drawDbScale(g, scaleBounds);
    drawMeterBar(g, rightBounds, smoothedPeakR, smoothedRmsR, peakHoldR, "R");
}

void ReticleEditor::drawMeterBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                 float peak, float rms, float peakHold,
                                 const juce::String& label)
{
    float floor = processor.dbFloor.load();

    // Channel label at top
    g.setColour(Neo::text);
    g.setFont(juce::FontOptions(13.0f));
    auto labelArea = bounds.removeFromTop(20.0f);
    g.drawText(label, labelArea, juce::Justification::centred, true);
    bounds.removeFromTop(4.0f);

    // dB readout at bottom
    float peakDb = juce::Decibels::gainToDecibels(peak, floor);
    auto readoutArea = bounds.removeFromBottom(18.0f);
    g.setColour(peakDb > -6.0f ? Neo::yellow : Neo::textDim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(juce::String(peakDb, 1) + " dB", readoutArea, juce::Justification::centred, true);
    bounds.removeFromBottom(4.0f);

    // Meter well (inset)
    Neo::drawInset(g, bounds, 6.0f);

    auto meterArea = bounds.reduced(3.0f);
    float meterH = meterArea.getHeight();

    // Normalize values
    auto normDb = [&](float gain) -> float
    {
        float db = juce::Decibels::gainToDecibels(gain, floor);
        return juce::jmap(db, floor, 0.0f, 0.0f, 1.0f);
    };

    float rmsNorm  = normDb(rms);
    float peakNorm = normDb(peak);
    float holdNorm = normDb(peakHold);

    // RMS bar — gradient fill
    float rmsH = meterH * rmsNorm;
    if (rmsH > 0.5f)
    {
        auto rmsBounds = juce::Rectangle<float>(
            meterArea.getX(), meterArea.getBottom() - rmsH,
            meterArea.getWidth(), rmsH);

        juce::ColourGradient rmsGrad(Neo::green, rmsBounds.getBottomLeft(),
                                     Neo::yellow, rmsBounds.getTopLeft(), false);
        if (rmsNorm > 0.7f)
            rmsGrad.addColour(0.85, Neo::red.withAlpha(0.8f));
        g.setGradientFill(rmsGrad);
        g.fillRoundedRectangle(rmsBounds, 3.0f);

        // Soft glow on RMS
        g.setColour(Neo::green.withAlpha(0.08f));
        g.fillRoundedRectangle(rmsBounds.expanded(2.0f), 4.0f);
    }

    // Peak bar — narrow center overlay
    float peakH = meterH * peakNorm;
    if (peakH > 0.5f)
    {
        float peakW = juce::jmax(4.0f, meterArea.getWidth() * 0.15f);
        auto peakBounds = juce::Rectangle<float>(
            meterArea.getCentreX() - peakW * 0.5f,
            meterArea.getBottom() - peakH,
            peakW, peakH);

        auto peakCol = peakNorm > 0.9f ? Neo::red : peakNorm > 0.7f ? Neo::yellow : Neo::green.brighter(0.3f);
        g.setColour(peakCol.withAlpha(0.9f));
        g.fillRoundedRectangle(peakBounds, 2.0f);
    }

    // Peak hold indicator — horizontal line
    if (holdNorm > 0.01f)
    {
        float holdY = meterArea.getBottom() - meterH * holdNorm;
        auto holdCol = holdNorm > 0.9f ? Neo::red : holdNorm > 0.7f ? Neo::yellow : Neo::accent;
        g.setColour(holdCol.withAlpha(0.85f));
        g.fillRect(meterArea.getX() + 2.0f, holdY - 1.0f, meterArea.getWidth() - 4.0f, 2.0f);
    }
}

void ReticleEditor::drawDbScale(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float floor = processor.dbFloor.load();
    auto scaleArea = bounds.withTrimmedTop(24.0f).withTrimmedBottom(22.0f);
    float h = scaleArea.getHeight();

    g.setFont(juce::FontOptions(9.0f));

    for (float db : { 0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f, -96.0f })
    {
        if (db < floor) continue;
        float norm = juce::jmap(db, floor, 0.0f, 0.0f, 1.0f);
        float y = scaleArea.getBottom() - h * norm;

        g.setColour(Neo::shadowDk.withAlpha(0.3f));
        g.fillRect(scaleArea.getX(), y, scaleArea.getWidth(), 0.5f);

        g.setColour(Neo::textDim);
        g.drawText(juce::String((int)db),
                   (int)scaleArea.getX(), (int)(y - 6), (int)scaleArea.getWidth(), 12,
                   juce::Justification::centred, true);
    }
}

void ReticleEditor::drawSettingsPanel(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Separator line
    g.setColour(Neo::shadowDk);
    g.fillRect(bounds.getX(), bounds.getY(), 1.0f, bounds.getHeight());

    // Panel background
    auto panelBg = bounds.withTrimmedLeft(1.0f);
    g.setColour(Neo::bgDark);
    g.fillRect(panelBg);

    // Panel title
    g.setColour(Neo::textDim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("TUNING", panelBg.removeFromTop(30.0f).reduced(12.0f, 0),
               juce::Justification::centredLeft, true);
}

void ReticleEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(kHeaderH);

    // Settings toggle in header
    settingsToggle.setBounds(header.removeFromRight(60).reduced(8, 8));

    if (settingsOpen)
    {
        auto panel = area.removeFromRight(kSettingsPanelW);
        panel.removeFromTop(32); // below "TUNING" title
        panel = panel.reduced(12, 0);

        struct SliderRow { juce::Slider& s; juce::Label& l; };
        SliderRow rows[] = {
            { attackSlider,   attackLabel },
            { decaySlider,    decayLabel },
            { peakHoldSlider, peakHoldLabel },
            { dbFloorSlider,  dbFloorLabel },
            { fpsSlider,      fpsLabel }
        };

        for (auto& row : rows)
        {
            auto labelBounds = panel.removeFromTop(16);
            row.l.setBounds(labelBounds);
            row.l.setVisible(true);

            auto sliderBounds = panel.removeFromTop(28);
            row.s.setBounds(sliderBounds);
            row.s.setVisible(true);

            panel.removeFromTop(8); // spacing
        }
    }
    else
    {
        // Hide all settings controls
        attackSlider.setVisible(false);   attackLabel.setVisible(false);
        decaySlider.setVisible(false);    decayLabel.setVisible(false);
        peakHoldSlider.setVisible(false); peakHoldLabel.setVisible(false);
        dbFloorSlider.setVisible(false);  dbFloorLabel.setVisible(false);
        fpsSlider.setVisible(false);      fpsLabel.setVisible(false);
    }
}
