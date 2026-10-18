#include "MixerStrip.h"
#include "../Theme.h"
#include "../../core/IDs.h"

namespace
{
    juce::Colour trackColourOf (const juce::ValueTree& t)
    {
        return juce::Colour ((juce::uint32) (juce::int64) t.getProperty (IDs::colour, juce::var ((juce::int64) 0xff64b5f6)));
    }
}

MixerStrip::MixerStrip (juce::ValueTree track, ProjectState& stateToUse, AudioEngine& engineToUse)
    : tree (track), trackTree (track), state (stateToUse), engine (engineToUse),
      slotIndex ((int) track.getProperty (IDs::mixerSlot, -1)), master (false)
{
    commonSetup();
    tree.addListener (this);
    refreshFromTree();
}

MixerStrip::MixerStrip (juce::ValueTree mixer, bool, ProjectState& stateToUse, AudioEngine& engineToUse)
    : tree (mixer), state (stateToUse), engine (engineToUse), master (true)
{
    commonSetup();
    tree.addListener (this);
    refreshFromTree();
}

MixerStrip::~MixerStrip()
{
    tree.removeListener (this);
}

void MixerStrip::commonSetup()
{
    nameLabel.setFont (Theme::font (11.5f, true));
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    slotLabel.setFont (Theme::font (9.5f));
    slotLabel.setJustificationType (juce::Justification::centred);
    slotLabel.setColour (juce::Label::textColourId, Theme::textDim());
    slotLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slotLabel);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setPopupDisplayEnabled (true, true, this);
    fader.setTextValueSuffix (" dB");
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.setRange (-60.0, 6.0, 0.1);
    fader.onDragStart = [this] { state.undo.beginNewTransaction (master ? "Master fader" : "Channel fader"); };
    fader.onValueChange = [this]
    {
        if (ignoreUiCallbacks)
            return;

        const auto prop = master ? juce::Identifier (IDs::masterGain) : juce::Identifier (IDs::gain);
        state.setProperty (tree, prop, (float) fader.getValue(),
                           master ? "Master fader" : "Channel fader", 1);   // coalesce dentro del drag
    };
    addAndMakeVisible (fader);

    panKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    panKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panKnob.setPopupDisplayEnabled (true, true, this);
    panKnob.setRange (-1.0, 1.0, 0.01);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.textFromValueFunction = [] (double v)
    {
        if (std::abs (v) < 0.01) return juce::String ("C");
        return juce::String ((int) std::round (std::abs (v) * 100.0)).retainCharacters ("0123456789")
                 + (v < 0.0 ? " L" : " R");
    };
    panKnob.onDragStart = [this] { state.undo.beginNewTransaction ("Pan"); };
    panKnob.onValueChange = [this]
    {
        if (ignoreUiCallbacks || master)
            return;

        state.setProperty (tree, IDs::pan, (float) panKnob.getValue(), "Pan", 1);
    };
    panKnob.setEnabled (! master);
    addAndMakeVisible (panKnob);

    vuMeter = std::make_unique<VUMeter>();
    addAndMakeVisible (*vuMeter);

    if (master)
    {
        spectrum.setAnalyzer (&engine.masterSpectrum());
        addAndMakeVisible (spectrum);
    }

    if (! master)
    {
        proChannel = std::make_unique<ProChannelPanel> (trackTree, state, engine);
        addAndMakeVisible (*proChannel);

        muteButton.setClickingTogglesState (true);
        muteButton.setColour (juce::TextButton::buttonColourId, Theme::accent2());
        muteButton.onClick = [this]
        {
            if (! ignoreUiCallbacks)
                state.setProperty (tree, IDs::mute, muteButton.getToggleState(), "Mute track");
        };
        addAndMakeVisible (muteButton);

        soloButton.setClickingTogglesState (true);
        soloButton.setColour (juce::TextButton::buttonColourId, Theme::danger());
        soloButton.onClick = [this]
        {
            if (! ignoreUiCallbacks)
                state.setProperty (tree, IDs::solo, soloButton.getToggleState(), "Solo track");
        };
        addAndMakeVisible (soloButton);
    }
}

void MixerStrip::paint (juce::Graphics& g)
{
    g.fillAll (master ? Theme::panelHi() : Theme::panelAlt());

    if (! master)
    {
        g.setColour (trackColourOf (trackTree));
        g.fillRect (0, 0, getWidth(), 3);
    }
    else
    {
        g.setColour (Theme::accent());
        g.fillRect (0, 0, getWidth(), 3);
    }

    g.setColour (Theme::border());
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void MixerStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 5);
    r.removeFromTop (3);

    nameLabel.setBounds (r.removeFromTop (16));
    slotLabel.setBounds (r.removeFromTop (12));
    r.removeFromTop (4);

    // --- Espectro FFT (sólo master) ---------------------------------
    if (master)
    {
        auto specArea = r.removeFromTop (80);
        spectrum.setBounds (specArea);
        r.removeFromTop (6);
    }

    // --- ProChannel (sólo tiras normales) ---------------------------
    if (! master && proChannel != nullptr)
    {
        auto pcArea = r.removeFromTop (ProChannelPanel::preferredHeight);
        proChannel->setBounds (pcArea);
        r.removeFromTop (6);
    }

    auto buttons = r.removeFromBottom (22);
    if (! master)
    {
        soloButton.setBounds (buttons.removeFromRight (24));
        buttons.removeFromRight (3);
        muteButton.setBounds (buttons.removeFromRight (24));
        r.removeFromBottom (4);
    }

    auto knobArea = r.removeFromBottom (40);
    panKnob.setBounds (knobArea.withSizeKeepingCentre (40, 40));
    r.removeFromBottom (6);

    // --- VU meter (izquierda del fader) ------------------------------
    auto meterArea = r.removeFromLeft (22);
    vuMeter->setBounds (meterArea);
    r.removeFromLeft (4);

    fader.setBounds (r);
}

void MixerStrip::refreshMeter()
{
    if (master)
    {
        vuMeter->setLevel (engine.masterRmsValue(), true);
        spectrum.refresh();
        return;
    }

    if (! juce::isPositiveAndBelow (slotIndex, AudioEngine::kMaxSlots))
    {
        vuMeter->setLevel (0.0f, false);
        return;
    }

    const auto& slot = engine.getSlot (slotIndex);
    vuMeter->setLevel (slot.rms.load (std::memory_order_relaxed),
                       slot.active.load (std::memory_order_relaxed));

    // Actualiza GR meter del ProChannel
    if (proChannel != nullptr)
    {
        const float grDb = engine.getProChannel (slotIndex).grDb.load (std::memory_order_relaxed);
        const float gr01 = grDb < -0.1f ? juce::jlimit (0.0f, 1.0f, -grDb / 24.0f) : 0.0f;
        proChannel->setGainReduction (gr01);
        proChannel->repaint();
    }
}

void MixerStrip::tickVU (double dt)
{
    vuMeter->tick (dt);
    vuMeter->repaint();
}

void MixerStrip::refreshFromTree()
{
    ignoreUiCallbacks = true;

    if (master)
    {
        nameLabel.setText ("MASTER", juce::dontSendNotification);
        slotLabel.setText ("Main Out", juce::dontSendNotification);

        const float db = (float) tree.getProperty (IDs::masterGain, 0.0f);
        if (fader.getValue() != (double) db)
            fader.setValue (db, juce::dontSendNotification);
    }
    else
    {
        nameLabel.setText (tree.getProperty (IDs::name).toString(), juce::dontSendNotification);
        slotLabel.setText ("Ch " + juce::String (slotIndex + 1), juce::dontSendNotification);

        const float db = (float) tree.getProperty (IDs::gain, 0.0f);
        if (fader.getValue() != (double) db)
            fader.setValue (db, juce::dontSendNotification);

        const float p = (float) tree.getProperty (IDs::pan, 0.0f);
        if (panKnob.getValue() != (double) p)
            panKnob.setValue (p, juce::dontSendNotification);

        muteButton.setToggleState ((bool) tree.getProperty (IDs::mute, false), juce::dontSendNotification);
        soloButton.setToggleState ((bool) tree.getProperty (IDs::solo, false), juce::dontSendNotification);
    }

    ignoreUiCallbacks = false;
    repaint();
}

void MixerStrip::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refreshFromTree();
}
