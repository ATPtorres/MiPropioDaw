#include "VUMeter.h"
#include "../Theme.h"

void VUMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Fondo del meter (crema/ámbar estilo analógico)
    g.setColour (juce::Colour (0xff2a2218));
    g.fillRoundedRectangle (r, 2.0f);

    g.setColour (juce::Colour (0xff3d3428));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);

    // Zona roja (+3 VU)
    {
        auto red = r.withTrimmedLeft (r.getWidth() * 0.82f).reduced (2.0f, 3.0f);
        g.setColour (juce::Colour (0xff8b1a1a).withAlpha (0.5f));
        g.fillRoundedRectangle (red, 1.0f);
    }

    // Marcas de escala
    const auto pivotX = r.getCentreX();
    const auto pivotY = r.getBottom() - 4.0f;
    const auto radius = r.getHeight() * 0.72f;

    g.setColour (juce::Colour (0xffc8b890));
    g.setFont (Theme::font (6.0f));

    // marcas: -20 -10 -7 -5 -3 -2 -1 0 +1 +2 +3 VU
    static const float marks[] = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f,
                                    0.0f,   1.0f,   2.0f,  3.0f };
    static const char* labels[] = { "20", "10", "7", "5", "3", "2", "1", "0", "+1", "+2", "+3" };

    const float arcStart = -0.72f;  // rad
    const float arcEnd   =  0.72f;

    for (size_t i = 0; i < sizeof (marks) / sizeof (marks[0]); ++i)
    {
        const float t = juce::jmap (marks[i], -20.0f, 3.0f, 0.0f, 1.0f);
        const float a = arcStart + t * (arcEnd - arcStart);
        const float cx = pivotX + std::sin (a) * radius;
        const float cy = pivotY - std::cos (a) * radius;

        const float tickLen = (marks[i] == 0.0f) ? 5.0f : 3.0f;
        const float tx = pivotX + std::sin (a) * (radius - tickLen);
        const float ty = pivotY - std::cos (a) * (radius - tickLen);

        g.setColour (marks[i] >= 0.0f ? juce::Colour (0xffe8a0a0) : juce::Colour (0xffc8b890));
        g.drawLine (tx, ty, cx, cy, 0.8f);

        // etiqueta (solo algunas)
        if (i % 2 == 0 || marks[i] == 0.0f)
        {
            const float lx = pivotX + std::sin (a) * (radius - 10.0f);
            const float ly = pivotY - std::cos (a) * (radius - 10.0f);
            g.drawText (labels[i], (int) (lx - 8), (int) (ly - 4), 16, 8,
                        juce::Justification::centred, false);
        }
    }

    // Aguja
    {
        const float a = arcStart + juce::jmap (angle, -1.0f, 1.0f, 0.0f, 1.0f) * (arcEnd - arcStart);
        const float tipX = pivotX + std::sin (a) * (radius - 2.0f);
        const float tipY = pivotY - std::cos (a) * (radius - 2.0f);

        g.setColour (juce::Colour (0xfff5e6c8));
        g.drawLine (pivotX, pivotY, tipX, tipY, 1.2f);

        // pivote
        g.setColour (juce::Colour (0xff8a7a60));
        g.fillEllipse (pivotX - 2.5f, pivotY - 2.5f, 5.0f, 5.0f);
    }
}
