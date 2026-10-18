#pragma once

#include <JuceHeader.h>

// Botón de transporte con icono vectorial (play / stop / record).
class TransportButton : public juce::Button
{
public:
    enum Type { play, stop, record };

    explicit TransportButton (Type typeToUse, const juce::String& accessibleName)
        : juce::Button (accessibleName), type (typeToUse) {}

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

private:
    Type type;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportButton)
};

// Medidor pequeño de CPU con barra.
class CpuBar : public juce::Component
{
public:
    void setPercent (float newPercent)   { percent = newPercent; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    float percent = 0.0f;
};
