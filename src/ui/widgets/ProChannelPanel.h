#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../audio/AudioEngine.h"
#include "../Theme.h"

// Panel ProChannel: EQ (3 bandas), Compresor, Saturación de tubo.
// Se apila dentro de cada MixerStrip. Escribe en el ValueTree del track
// (subtree proChannel) con undo + coalesce para knobs arrastrables.
class ProChannelPanel : public juce::Component,
                        private juce::ValueTree::Listener
{
public:
    ProChannelPanel (juce::ValueTree trackTree, ProjectState& state, AudioEngine& engine);
    ~ProChannelPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Actualiza el medidor de GR (0..1) desde el slot del motor
    void setGainReduction (float gr01)
    {
        grMeterValue = juce::jlimit (0.0f, 1.0f, gr01);
    }

    // Altura mínima necesaria para todos los controles
    static constexpr int preferredHeight = 430;

private:
    // ------------------------------------------------------------------
    // Knob compacto: Slider rotary + Label debajo
    // ------------------------------------------------------------------
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;

        Knob (const juce::String& text, const juce::String& suffix = {})
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setPopupDisplayEnabled (true, true, nullptr);
            slider.setTextValueSuffix (suffix);

            label.setText (text, juce::dontSendNotification);
            label.setFont (Theme::font (8.5f));
            label.setColour (juce::Label::textColourId, Theme::textDim());
            label.setJustificationType (juce::Justification::centred);
            label.setInterceptsMouseClicks (false, false);
        }

        void add (juce::Component& parent)
        {
            parent.addAndMakeVisible (slider);
            parent.addAndMakeVisible (label);
        }

        void setBounds (juce::Rectangle<int> area)
        {
            label.setBounds (area.removeFromBottom (11));
            slider.setBounds (area);
        }
    };

    // ------------------------------------------------------------------
    // Botón de encendido de sección (toggle pequeño)
    // ------------------------------------------------------------------
    struct SectionButton
    {
        juce::TextButton button;
        juce::Label      title;

        SectionButton (const juce::String& name)
        {
            button.setButtonText ({});
            button.setClickingTogglesState (true);
            button.setColour (juce::TextButton::buttonColourId, Theme::accent().withAlpha (0.25f));
            button.setColour (juce::TextButton::buttonOnColourId,  Theme::accent());
            button.setColour (juce::TextButton::textColourOffId, Theme::textDim());
            button.setColour (juce::TextButton::textColourOnId,  juce::Colours::white);

            title.setText (name, juce::dontSendNotification);
            title.setFont (Theme::font (9.5f, true));
            title.setColour (juce::Label::textColourId, Theme::text());
            title.setJustificationType (juce::Justification::centredLeft);
            title.setInterceptsMouseClicks (false, false);
        }

        void add (juce::Component& parent)
        {
            parent.addAndMakeVisible (button);
            parent.addAndMakeVisible (title);
        }
    };

    void refreshFromTree();
    void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyChanged,
                                   const juce::Identifier& property) override;

    void setupKnob (Knob& k, const juce::Identifier& prop,
                    double minV, double maxV, double interval,
                    std::function<void(double)> onChange = {});

    juce::ValueTree tree;       // track
    juce::ValueTree pc;         // proChannel subtree
    ProjectState& state;
    AudioEngine& engine;
    int slotIndex = -1;
    bool ignoreUi = false;

    // --- EQ -----------------------------------------------------------
    SectionButton eqSection  { "EQ" };
    Knob eqLowGain  { "Low G",  " dB" };
    Knob eqLowFreq  { "Low F",  " Hz" };
    Knob eqMidGain  { "Mid G",  " dB" };
    Knob eqMidFreq  { "Mid F",  " Hz" };
    Knob eqMidQ     { "Q" };
    Knob eqHighGain { "High G", " dB" };
    Knob eqHighFreq { "High F", " Hz" };

    // --- Compressor ---------------------------------------------------
    SectionButton compSection { "COMP" };
    Knob compThreshold { "Thr",  " dB" };
    Knob compRatio     { "Ratio", ":1" };
    Knob compAttack    { "Atk",  " ms" };
    Knob compRelease   { "Rel",  " ms" };
    Knob compMakeup    { "Mkup", " dB" };

    // GR meter horizontal
    float grMeterValue = 0.0f;   // 0..1 (1 = máxima reducción)

    // --- Saturation ---------------------------------------------------
    SectionButton satSection { "SAT" };
    Knob satDrive  { "Drive" };
    Knob satBias   { "Bias" };
    Knob satOutput { "Out", " dB" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProChannelPanel)
};
