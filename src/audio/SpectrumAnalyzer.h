#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

// Analizador FFT de espectro para el canal master.
// El hilo de audio empuja muestras; el hilo de mensaje lee el espectro suavizado.
class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer() = default;

    static constexpr int fftOrder = 11;                       // 2048 puntos
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int numBins  = fftSize / 2;

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi
                                                          * (float) i / (float) (fftSize - 1)));

        buffer.assign ((size_t) fftSize, 0.0f);
        magnitudes.assign ((size_t) numBins, -100.0f);
        smoothed.assign ((size_t) numBins, -100.0f);
        // performFrequencyOnlyForwardTransform necesita 2*fftSize floats
        fftData.assign ((size_t) fftSize * 2, 0.0f);
        writePos = 0;
        samplesSinceLastFFT = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        std::fill (magnitudes.begin(), magnitudes.end(), -100.0f);
        std::fill (smoothed.begin(), smoothed.end(), -100.0f);
        writePos = 0;
        samplesSinceLastFFT = 0;
    }

    // Llamar desde el hilo de audio con la señal mono del master.
    void push (const float* data, int numSamples)
    {
        if (data == nullptr || numSamples <= 0 || buffer.empty())
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            buffer[(size_t) writePos] = data[i];
            writePos = (writePos + 1) % fftSize;
            ++samplesSinceLastFFT;
        }

        // Calcula FFT cada fftSize muestras (hop = fftSize, sin overlap)
        if (samplesSinceLastFFT >= fftSize)
        {
            samplesSinceLastFFT = 0;
            computeFFT();
        }
    }

    // Devuelve magnitudes en dB (hasta numBins). Copia al buffer dado.
    // Devuelve true si hay datos válidos.
    bool getSpectrum (float* dest, int destSize) const
    {
        if (dest == nullptr || destSize <= 0 || smoothed.empty())
            return false;

        const int n = juce::jmin (destSize, numBins);
        for (int i = 0; i < n; ++i)
            dest[i] = smoothed[(size_t) i];

        return true;
    }

    // Frecuencia (Hz) para un bin dado.
    double binToHz (int bin) const
    {
        return (double) bin * sr / (double) fftSize;
    }

    static constexpr double minDb = -100.0;
    static constexpr double maxDb = 0.0;

private:
    void computeFFT()
    {
        // Copia temporal ordenada con ventana Hann en la primera mitad
        for (int i = 0; i < fftSize; ++i)
        {
            const int src = (writePos + i) % fftSize;
            fftData[(size_t) i] = buffer[(size_t) src] * window[(size_t) i];
        }
        // segunda mitad (parte imaginaria) a cero
        for (int i = fftSize; i < fftSize * 2; ++i)
            fftData[(size_t) i] = 0.0f;

        // FFT real → magnitudes en dB
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        // Normalización: window sum × 2 para one-sided
        const float norm = 2.0f / (float) fftSize;

        for (int i = 0; i < numBins; ++i)
        {
            const float mag = fftData[(size_t) i] * norm;
            const float db  = juce::jmax ((float) minDb, juce::Decibels::gainToDecibels (mag, (float) minDb));
            magnitudes[(size_t) i] = db;

            // suavizado exponencial (ataque rápido, caída lenta)
            const float prev = smoothed[(size_t) i];
            const float coeff = db > prev ? 0.6f : 0.08f;
            smoothed[(size_t) i] = prev + coeff * (db - prev);
        }
    }

    double sr = 48000.0;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window;
    std::vector<float> buffer;
    std::vector<float> fftData;
    std::vector<float> magnitudes;
    std::vector<float> smoothed;
    int writePos = 0;
    int samplesSinceLastFFT = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
