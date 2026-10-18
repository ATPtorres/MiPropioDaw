#include "MidiEditorPanel.h"
#include "../Theme.h"

void MidiEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panelAlt());

    const int keyW   = 64;
    const int rowH   = 14;
    const int noteH  = 14;

    // teclado izquierdo
    g.setColour (Theme::panel());
    g.fillRect (0, 0, keyW, getHeight());

    for (int i = 0; i * rowH < getHeight(); ++i)
    {
        const int y = i * rowH;
        const int note = (127 - i) % 12;
        const bool black = (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);

        g.setColour (black ? juce::Colour (0xff161616) : juce::Colour (0xffd0d0d0));
        g.fillRect (0, y, keyW, rowH - 1);

        g.setColour (Theme::border());
        g.fillRect (keyW, y, getWidth() - keyW, 1);
    }

    // rejilla de pulsos
    const int gridStart = keyW;
    const int beatW = Theme::pxPerBeat;

    for (int x = gridStart; x < getWidth(); x += beatW)
    {
        const int beat = (x - gridStart) / beatW;
        const bool bar = beat % 4 == 0;

        g.setColour (bar ? Theme::gridLineStrong() : Theme::gridLine());
        g.fillRect (x, 0, bar ? 2 : 1, getHeight());
    }

    // filas del piano roll
    for (int y = 0; y < getHeight(); y += noteH)
    {
        const int n = (127 - y / noteH) % 12;
        const bool black = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
        if (black)
        {
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRect (gridStart, y, getWidth() - gridStart, noteH);
        }
    }

    // aviso de fase
    auto box = juce::Rectangle<int> (getWidth() / 2 - 190, getHeight() / 2 - 34, 380, 68);
    g.setColour (juce::Colour (0xff141414).withAlpha (0.88f));
    g.fillRoundedRectangle (box.toFloat(), 8.0f);
    g.setColour (Theme::border());
    g.drawRoundedRectangle (box.toFloat(), 8.0f, 1.0f);

    g.setColour (Theme::accent());
    g.setFont (Theme::font (14.0f, true));
    g.drawText ("EDITOR MIDI", box.removeFromTop (30), juce::Justification::centred);

    g.setColour (Theme::textDim());
    g.setFont (Theme::font (12.0f));
    g.drawText ("Herramientas, Scale Highlighting y Ghost Notes — Fase 3",
                box, juce::Justification::centred);
}
