#include "TransportWidgets.h"
#include "../Theme.h"

void TransportButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    auto colour = on ? findColour (juce::TextButton::buttonColourId) : Theme::buttonOff();
    if (down)      colour = colour.brighter (0.25f);
    else if (highlighted) colour = colour.brighter (0.12f);

    g.setColour (colour);
    g.fillRoundedRectangle (r, 4.0f);

    g.setColour (on ? juce::Colours::transparentBlack : Theme::border());
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    auto iconColour = on ? juce::Colours::white : Theme::text();
    if (! isEnabled())
        iconColour = iconColour.withAlpha (0.35f);

    g.setColour (iconColour);

    auto iconArea = getLocalBounds().toFloat().reduced ((float) getWidth() * 0.28f);

    switch (type)
    {
        case play:
        {
            juce::Path p;
            p.startNewSubPath (iconArea.getX(), iconArea.getY());
            p.lineTo (iconArea.getRight(), iconArea.getCentreY());
            p.lineTo (iconArea.getX(), iconArea.getBottom());
            p.closeSubPath();
            g.fillPath (p);
            break;
        }
        case stop:
            g.fillRoundedRectangle (iconArea, 1.5f);
            break;
        case record:
            g.fillEllipse (iconArea);
            break;
    }
}

// ---------------------------------------------------------------------------
void CpuBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    auto textArea = r.removeFromTop (r.getHeight() * 0.55f);

    g.setColour (percent > 70.0f ? Theme::accent2() : Theme::textDim());
    g.setFont (Theme::font (10.5f));
    g.drawText ("CPU " + juce::String (percent, 1) + "%", textArea, juce::Justification::centredLeft);

    auto barArea = r.removeFromTop (4.0f).withWidth ((float) getWidth() - 2.0f);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillRoundedRectangle (barArea, 2.0f);

    auto filled = barArea.withWidth (barArea.getWidth() * juce::jlimit (0.0f, 1.0f, percent / 100.0f));
    g.setColour (percent > 70.0f ? Theme::danger() : Theme::accent());
    g.fillRoundedRectangle (filled, 2.0f);
}
