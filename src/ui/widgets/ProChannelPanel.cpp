#include "ProChannelPanel.h"
#include "../Theme.h"
#include "../../core/IDs.h"

ProChannelPanel::ProChannelPanel (juce::ValueTree trackTree, ProjectState& stateToUse, AudioEngine& engineToUse)
    : tree (trackTree), state (stateToUse), engine (engineToUse),
      slotIndex ((int) trackTree.getProperty (IDs::mixerSlot, -1))
{
    pc = tree.getChildWithName (IDs::proChannel);
    if (! pc.isValid())
    {
        // crea el subtree si no existe (proyectos viejos)
        pc = juce::ValueTree (IDs::proChannel);
        pc.setProperty (IDs::pcEqEnabled,   false, nullptr);
        pc.setProperty (IDs::pcEqLowGain,   0.0f,  nullptr);
        pc.setProperty (IDs::pcEqLowFreq,   80.0f, nullptr);
        pc.setProperty (IDs::pcEqMidGain,   0.0f,  nullptr);
        pc.setProperty (IDs::pcEqMidFreq,   1000.0f, nullptr);
        pc.setProperty (IDs::pcEqMidQ,      0.7f,  nullptr);
        pc.setProperty (IDs::pcEqHighGain,  0.0f,  nullptr);
        pc.setProperty (IDs::pcEqHighFreq,  8000.0f, nullptr);
        pc.setProperty (IDs::pcCompEnabled,   false, nullptr);
        pc.setProperty (IDs::pcCompThreshold, -18.0f, nullptr);
        pc.setProperty (IDs::pcCompRatio,     4.0f,  nullptr);
        pc.setProperty (IDs::pcCompAttack,    10.0f, nullptr);
        pc.setProperty (IDs::pcCompRelease,   100.0f, nullptr);
        pc.setProperty (IDs::pcCompMakeup,    0.0f,  nullptr);
        pc.setProperty (IDs::pcSatEnabled, false, nullptr);
        pc.setProperty (IDs::pcSatDrive,   0.3f,  nullptr);
        pc.setProperty (IDs::pcSatBias,    0.15f, nullptr);
        pc.setProperty (IDs::pcSatOutput,  0.0f,  nullptr);
        tree.addChild (pc, -1, nullptr);
    }

    // --- EQ ---
    eqSection.add (*this);
    eqSection.button.onClick = [this]
    {
        if (! ignoreUi)
            state.setProperty (pc, IDs::pcEqEnabled, eqSection.button.getToggleState(), "EQ enable");
    };
    eqLowGain.add (*this);  setupKnob (eqLowGain,  IDs::pcEqLowGain,  -15.0, 15.0, 0.1);
    eqLowFreq.add (*this);  setupKnob (eqLowFreq,  IDs::pcEqLowFreq,   20.0, 500.0, 1.0);
    eqMidGain.add (*this);  setupKnob (eqMidGain,  IDs::pcEqMidGain,  -15.0, 15.0, 0.1);
    eqMidFreq.add (*this);  setupKnob (eqMidFreq,  IDs::pcEqMidFreq,  100.0, 12000.0, 1.0);
    eqMidQ.add (*this);     setupKnob (eqMidQ,     IDs::pcEqMidQ,      0.2,  8.0, 0.01);
    eqHighGain.add (*this); setupKnob (eqHighGain, IDs::pcEqHighGain, -15.0, 15.0, 0.1);
    eqHighFreq.add (*this); setupKnob (eqHighFreq, IDs::pcEqHighFreq, 1000.0, 20000.0, 1.0);

    // --- Comp ---
    compSection.add (*this);
    compSection.button.onClick = [this]
    {
        if (! ignoreUi)
            state.setProperty (pc, IDs::pcCompEnabled, compSection.button.getToggleState(), "Comp enable");
    };
    compThreshold.add (*this); setupKnob (compThreshold, IDs::pcCompThreshold, -60.0, 0.0, 0.1);
    compRatio.add (*this);     setupKnob (compRatio,     IDs::pcCompRatio,      1.0, 20.0, 0.1);
    compAttack.add (*this);    setupKnob (compAttack,    IDs::pcCompAttack,     0.1, 100.0, 0.1);
    compRelease.add (*this);   setupKnob (compRelease,   IDs::pcCompRelease,    5.0, 2000.0, 1.0);
    compMakeup.add (*this);    setupKnob (compMakeup,    IDs::pcCompMakeup,   -12.0, 12.0, 0.1);

    // --- Sat ---
    satSection.add (*this);
    satSection.button.onClick = [this]
    {
        if (! ignoreUi)
            state.setProperty (pc, IDs::pcSatEnabled, satSection.button.getToggleState(), "Sat enable");
    };
    satDrive.add (*this);  setupKnob (satDrive,  IDs::pcSatDrive,  0.0, 1.0, 0.01);
    satBias.add (*this);   setupKnob (satBias,   IDs::pcSatBias,  -0.5, 0.5, 0.01);
    satOutput.add (*this); setupKnob (satOutput, IDs::pcSatOutput, -12.0, 12.0, 0.1);

    pc.addListener (this);
    refreshFromTree();
}

ProChannelPanel::~ProChannelPanel()
{
    pc.removeListener (this);
}

void ProChannelPanel::setupKnob (Knob& k, const juce::Identifier& prop,
                                 double minV, double maxV, double interval,
                                 std::function<void(double)>)
{
    k.slider.setRange (minV, maxV, interval);
    k.slider.onDragStart = [this]
    {
        state.undo.beginNewTransaction ("ProChannel");
    };
    k.slider.onValueChange = [this, &k, prop]
    {
        if (! ignoreUi)
            state.setProperty (pc, prop, (float) k.slider.getValue(), "ProChannel", 1);
    };
}

void ProChannelPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (Theme::panelHi());
    g.fillRoundedRectangle (r, 3.0f);

    g.setColour (Theme::border());
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

    // GR meter (detrás del header del comp)
    if (grMeterValue > 0.01f)
    {
        auto meterArea = juce::Rectangle<float> ((float) getX() + 62.0f, (float) (compSection.title.getBottom() + 2),
                                                  (float) getWidth() - 70.0f, 4.0f);
        g.setColour (juce::Colour (0xff3a2020));
        g.fillRoundedRectangle (meterArea, 1.0f);

        const float w = meterArea.getWidth() * juce::jlimit (0.0f, 1.0f, grMeterValue);
        g.setColour (grMeterValue > 0.7f ? Theme::danger() : Theme::accent2());
        g.fillRoundedRectangle (meterArea.withWidth (w), 1.0f);
    }
}

void ProChannelPanel::resized()
{
    auto r = getLocalBounds().reduced (6, 5);

    // --- EQ header + knobs ------------------------------------------
    auto eqHeader = r.removeFromTop (18);
    eqSection.button.setBounds (eqHeader.removeFromLeft (16));
    eqSection.title.setBounds (eqHeader);

    r.removeFromTop (2);

    const int colW = (r.getWidth() - 8) / 3;

    auto row1 = r.removeFromTop (54);
    eqLowGain.setBounds (row1.removeFromLeft (colW));
    row1.removeFromLeft (4);
    eqLowFreq.setBounds (row1.removeFromLeft (colW));

    auto row2 = r.removeFromTop (54);
    eqMidGain.setBounds (row2.removeFromLeft (colW));
    row2.removeFromLeft (4);
    eqMidFreq.setBounds (row2.removeFromLeft (colW));
    row2.removeFromLeft (4);
    eqMidQ.setBounds (row2);

    auto row3 = r.removeFromTop (54);
    eqHighGain.setBounds (row3.removeFromLeft (colW));
    row3.removeFromLeft (4);
    eqHighFreq.setBounds (row3.removeFromLeft (colW));

    r.removeFromTop (6);

    // --- Comp header + knobs ----------------------------------------
    auto compHeader = r.removeFromTop (18);
    compSection.button.setBounds (compHeader.removeFromLeft (16));
    compSection.title.setBounds (compHeader);

    r.removeFromTop (2);

    auto crow1 = r.removeFromTop (54);
    compThreshold.setBounds (crow1.removeFromLeft (colW));
    crow1.removeFromLeft (4);
    compRatio.setBounds (crow1.removeFromLeft (colW));
    crow1.removeFromLeft (4);
    compAttack.setBounds (crow1);

    auto crow2 = r.removeFromTop (54);
    compRelease.setBounds (crow2.removeFromLeft (colW));
    crow2.removeFromLeft (4);
    compMakeup.setBounds (crow2.removeFromLeft (colW));

    r.removeFromTop (6);

    // --- Sat header + knobs -----------------------------------------
    auto satHeader = r.removeFromTop (18);
    satSection.button.setBounds (satHeader.removeFromLeft (16));
    satSection.title.setBounds (satHeader);

    r.removeFromTop (2);

    auto srow = r.removeFromTop (54);
    satDrive.setBounds (srow.removeFromLeft (colW));
    srow.removeFromLeft (4);
    satBias.setBounds (srow.removeFromLeft (colW));
    srow.removeFromLeft (4);
    satOutput.setBounds (srow);
}

void ProChannelPanel::refreshFromTree()
{
    ignoreUi = true;

    eqSection.button.setToggleState ((bool) pc.getProperty (IDs::pcEqEnabled, false), juce::dontSendNotification);
    eqLowGain.slider.setValue  ((double) pc.getProperty (IDs::pcEqLowGain, 0.0f),  juce::dontSendNotification);
    eqLowFreq.slider.setValue  ((double) pc.getProperty (IDs::pcEqLowFreq, 80.0f), juce::dontSendNotification);
    eqMidGain.slider.setValue  ((double) pc.getProperty (IDs::pcEqMidGain, 0.0f),  juce::dontSendNotification);
    eqMidFreq.slider.setValue  ((double) pc.getProperty (IDs::pcEqMidFreq, 1000.0f), juce::dontSendNotification);
    eqMidQ.slider.setValue     ((double) pc.getProperty (IDs::pcEqMidQ, 0.7f),     juce::dontSendNotification);
    eqHighGain.slider.setValue ((double) pc.getProperty (IDs::pcEqHighGain, 0.0f), juce::dontSendNotification);
    eqHighFreq.slider.setValue ((double) pc.getProperty (IDs::pcEqHighFreq, 8000.0f), juce::dontSendNotification);

    compSection.button.setToggleState ((bool) pc.getProperty (IDs::pcCompEnabled, false), juce::dontSendNotification);
    compThreshold.slider.setValue ((double) pc.getProperty (IDs::pcCompThreshold, -18.0f), juce::dontSendNotification);
    compRatio.slider.setValue     ((double) pc.getProperty (IDs::pcCompRatio, 4.0f),      juce::dontSendNotification);
    compAttack.slider.setValue    ((double) pc.getProperty (IDs::pcCompAttack, 10.0f),    juce::dontSendNotification);
    compRelease.slider.setValue   ((double) pc.getProperty (IDs::pcCompRelease, 100.0f),  juce::dontSendNotification);
    compMakeup.slider.setValue    ((double) pc.getProperty (IDs::pcCompMakeup, 0.0f),     juce::dontSendNotification);

    satSection.button.setToggleState ((bool) pc.getProperty (IDs::pcSatEnabled, false), juce::dontSendNotification);
    satDrive.slider.setValue  ((double) pc.getProperty (IDs::pcSatDrive, 0.3f),  juce::dontSendNotification);
    satBias.slider.setValue   ((double) pc.getProperty (IDs::pcSatBias, 0.15f),  juce::dontSendNotification);
    satOutput.slider.setValue ((double) pc.getProperty (IDs::pcSatOutput, 0.0f), juce::dontSendNotification);

    ignoreUi = false;

    // sincroniza el DSP inmediatamente
    if (juce::isPositiveAndBelow (slotIndex, AudioEngine::kMaxSlots))
        engine.syncProChannelFromTree (tree);

    repaint();
}

void ProChannelPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refreshFromTree();
}
