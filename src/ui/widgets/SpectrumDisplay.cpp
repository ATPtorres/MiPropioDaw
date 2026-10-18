#include "SpectrumDisplay.h"
#include "../Theme.h"

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff0e0e0e));
    g.fillRoundedRectangle (r, 2.0f);

    g.setColour (Theme::border());
    g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);

    if (! hasData || analyzer == nullptr)
    {
        g.setColour (Theme::textDim().withAlpha (0.4f));
        g.setFont (Theme::font (8.0f));
        g.drawText ("SPECTRUM", getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Ejex logarítmico: 20 Hz → 20 kHz
    const double minHz = 20.0, maxHz = 20000.0;
    const float logMin = (float) std::log10 (minHz);
    const float logMax = (float) std::log10 (maxHz);
    const float logSpan = logMax - logMin;

    juce::Path path;
    bool started = false;

    const int w = getWidth();
    const int h = getHeight();

    for (int px = 0; px < w; ++px)
    {
        const float t = (float) px / (float) juce::jmax (1, w - 1);
        const float hz = std::pow (10.0f, logMin + t * logSpan);

        // bin = hz * fftSize / sr; binToHz(1) = sr / fftSize → bin = hz / binToHz(1)
        const float hzPerBin = (float) analyzer->binToHz (1);
        const int bin = juce::jlimit (1, SpectrumAnalyzer::numBins - 1, (int) (hz / hzPerBin));

        const float db = cache[(size_t) bin];
        const float norm = juce::jlimit (0.0f, 1.0f,
                                         (float) ((db - SpectrumAnalyzer::minDb)
                                                 / (SpectrumAnalyzer::maxDb - SpectrumAnalyzer::minDb)));
        const float y = (float) h - norm * (float) h * 0.92f;

        if (! started)
        {
            path.startNewSubPath ((float) px, y);
            started = true;
        }
        else
        {
            path.lineTo ((float) px, y);
        }
    }

    // relleno degradado
    auto fill = path;
    fill.lineTo ((float) w, (float) h);
    fill.lineTo (0.0f, (float) h);
    fill.closeSubPath();

    g.setColour (Theme::accent().withAlpha (0.18f));
    g.fillPath (fill);

    g.setColour (Theme::accent());
    g.strokePath (path, juce::PathStrokeType (1.2f));

    // línea de 0 dB
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (h - (int) ((0.0f - (float) SpectrumAnalyzer::minDb)
                                      / (float) (SpectrumAnalyzer::maxDb - SpectrumAnalyzer::minDb) * h * 0.92f),
                          0.0f, (float) w);
}
