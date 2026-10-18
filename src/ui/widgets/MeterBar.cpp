#include "MeterBar.h"
#include "../Theme.h"

namespace
{
    constexpr float kMinDb = -60.0f;
    constexpr float kMaxDb =  6.0f;
}

float MeterBar::yForGain (float gain) const
{
    const auto db = juce::Decibels::gainToDecibels (juce::jmax (0.0f, gain), kMinDb);
    const auto norm = (db - kMinDb) / (kMaxDb - kMinDb);
    return 1.0f - juce::jlimit (0.0f, 1.0f, norm);
}

void MeterBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRoundedRectangle (r, 2.0f);

    if (! active && peak <= 0.0f)
        return;

    // rms (columna interior)
    auto rmsTop = r.getY() + yForGain (rms) * r.getHeight();
    auto rmsArea = juce::Rectangle<float> (r.getX() + 2.0f, rmsTop, r.getWidth() - 4.0f, r.getBottom() - rmsTop);

    juce::ColourGradient grad (Theme::success(), rmsArea.getX(), rmsArea.getBottom(),
                               Theme::accent2(), rmsArea.getX(), rmsArea.getY(), false);
    grad.addColour (0.85, Theme::danger());
    g.setGradientFill (grad);
    g.fillRect (rmsArea);

    // pico (línea fina)
    peakHold = juce::jmax (peak, peakHold * 0.96f);

    auto peakY = r.getY() + yForGain (peak) * r.getHeight();
    auto holdY = r.getY() + yForGain (peakHold) * r.getHeight();

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillRect (r.getX() + 1.0f, peakY, r.getWidth() - 2.0f, 1.5f);

    g.setColour (Theme::accent2());
    g.fillRect (r.getX() + 1.0f, holdY, r.getWidth() - 2.0f, 1.0f);

    // línea de 0 dB
    auto zeroY = r.getY() + yForGain (juce::Decibels::decibelsToGain (0.0f)) * r.getHeight();
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillRect (r.getX(), zeroY, r.getWidth(), 1.0f);
}
