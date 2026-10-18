#pragma once

#include <JuceHeader.h>
#include "../../audio/SpectrumAnalyzer.h"

// Espectro FFT logarítmico para el canal master.
// Lee del SpectrumAnalyzer del motor en el timer del MixerView.
class SpectrumDisplay : public juce::Component
{
public:
    void setAnalyzer (SpectrumAnalyzer* a) noexcept { analyzer = a; }

    void refresh()
    {
        if (analyzer != nullptr)
        {
            analyzer->getSpectrum (cache.data(), (int) cache.size());
            hasData = true;
        }
        repaint();
    }

    void paint (juce::Graphics& g) override;

private:
    SpectrumAnalyzer* analyzer = nullptr;
    std::array<float, SpectrumAnalyzer::numBins> cache {};
    bool hasData = false;
};
