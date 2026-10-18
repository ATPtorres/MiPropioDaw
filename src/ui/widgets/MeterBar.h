#pragma once

#include <JuceHeader.h>

// Medidor vertical de nivel (pico + rms) con retención de pico.
class MeterBar : public juce::Component
{
public:
    void setLevels (float newPeak, float newRms, bool isActive)
    {
        peak = newPeak;
        rms  = newRms;
        active = isActive;
        repaint();
    }

    void paint (juce::Graphics& g) override;

private:
    float yForGain (float gain) const;

    float peak = 0.0f;
    float rms  = 0.0f;
    float peakHold = 0.0f;
    bool  active = false;
};
