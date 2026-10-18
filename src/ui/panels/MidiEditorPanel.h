#pragma once

#include <JuceHeader.h>

// Editor MIDI — esqueleto visual (funcionalidad completa en Fase 3).
class MidiEditorPanel : public juce::Component
{
public:
    MidiEditorPanel() = default;
    void paint (juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEditorPanel)
};
