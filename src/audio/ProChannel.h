#pragma once

#include <JuceHeader.h>
#include <atomic>

// Cadena DSP del ProChannel por canal:
//   EQ analógico (3 bandas biquad + saturación suave)
//   → Compresor feed-forward (soft knee, detector peak)
//   → Saturación de tubo británica (asymmetrical soft clip)
// Sin allocations en process(); parámetros como atomics (message thread escribe).
class ProChannelProcessor
{
public:
    ProChannelProcessor() = default;

    // --- parámetros (átomics) ---------------------------------------
    struct Params
    {
        // EQ
        std::atomic<bool>  eqEnabled   { false };
        std::atomic<float> eqLowGain   { 0.0f };   // dB
        std::atomic<float> eqLowFreq   { 80.0f };   // Hz
        std::atomic<float> eqMidGain   { 0.0f };   // dB
        std::atomic<float> eqMidFreq   { 1000.0f }; // Hz
        std::atomic<float> eqMidQ      { 0.7f };
        std::atomic<float> eqHighGain  { 0.0f };   // dB
        std::atomic<float> eqHighFreq  { 8000.0f }; // Hz

        // Compressor
        std::atomic<bool>  compEnabled   { false };
        std::atomic<float> compThreshold { -18.0f }; // dB
        std::atomic<float> compRatio     { 4.0f };
        std::atomic<float> compAttack    { 10.0f };  // ms
        std::atomic<float> compRelease   { 100.0f }; // ms
        std::atomic<float> compMakeup    { 0.0f };   // dB

        // Saturación
        std::atomic<bool>  satEnabled { false };
        std::atomic<float> satDrive   { 0.3f };  // 0..1
        std::atomic<float> satBias    { 0.15f }; // -0.5..0.5
        std::atomic<float> satOutput  { 0.0f };  // dB
    };

    Params params;

    // --- medición (hilo audio escribe, UI lee) ----------------------
    std::atomic<float> grDb { 0.0f };   // gain reduction actual (dB, negativo)

    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset()
    {
        for (auto& f : filters) f = {};
        env = 0.0f;
        grLin = 1.0f;
        grDb.store (0.0f, std::memory_order_relaxed);
    }

    // Procesa mono in-place.
    void process (float* data, int numSamples)
    {
        if (data == nullptr || numSamples <= 0)
            return;

        const bool useEq   = params.eqEnabled.load (std::memory_order_relaxed);
        const bool useComp = params.compEnabled.load (std::memory_order_relaxed);
        const bool useSat  = params.satEnabled.load (std::memory_order_relaxed);

        if (! useEq && ! useComp && ! useSat)
            return;

        if (useEq)
            updateEQ();

        float thresholdLin = 1.0f, ratioInv = 1.0f;
        float attackCoeff  = 0.0f, releaseCoeff = 0.0f;
        float makeupLin    = 1.0f;

        if (useComp)
        {
            const float thrDb = params.compThreshold.load (std::memory_order_relaxed);
            const float ratio = juce::jmax (1.0f, params.compRatio.load (std::memory_order_relaxed));
            const float atkMs = juce::jmax (0.05f, params.compAttack.load (std::memory_order_relaxed));
            const float relMs = juce::jmax (1.0f,  params.compRelease.load (std::memory_order_relaxed));
            const float mkDb  = params.compMakeup.load (std::memory_order_relaxed);

            thresholdLin = juce::Decibels::decibelsToGain (thrDb);
            ratioInv     = 1.0f / ratio;
            attackCoeff  = std::exp (-1.0f / (float) (atkMs * 0.001 * sr));
            releaseCoeff = std::exp (-1.0f / (float) (relMs * 0.001 * sr));
            makeupLin    = juce::Decibels::decibelsToGain (mkDb);
        }

        const float satDrive  = 0.5f + juce::jlimit (0.0f, 1.0f, params.satDrive.load (std::memory_order_relaxed)) * 4.5f;
        const float satBias   = juce::jlimit (-0.5f, 0.5f, params.satBias.load (std::memory_order_relaxed)) * 0.5f;
        const float satOutLin = juce::Decibels::decibelsToGain (params.satOutput.load (std::memory_order_relaxed));
        const float satTanhBias = std::tanh (satBias);

        float currentGrDb = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];

            // --- EQ --------------------------------------------------
            if (useEq)
            {
                for (auto& f : filters)
                    x = f.processSample (x);

                // modelado analógico: saturación suave post-filtro
                x = std::tanh (x * 1.15f) * 0.92f;
            }

            // --- Compresor -------------------------------------------
            if (useComp)
            {
                const float absX = std::abs (x);
                const float coeff = absX > env ? attackCoeff : releaseCoeff;
                env = absX + coeff * (env - absX);

                float gr = 1.0f;
                if (env > thresholdLin)
                {
                    const float overDb = juce::Decibels::gainToDecibels (env)
                                       - juce::Decibels::gainToDecibels (thresholdLin);
                    const float grDbVal = overDb * (ratioInv - 1.0f);
                    gr = juce::Decibels::decibelsToGain (grDbVal);
                }

                grLin = gr;
                x *= gr * makeupLin;

                currentGrDb = juce::Decibels::gainToDecibels (grLin);
            }

            // --- Saturación de tubo ----------------------------------
            if (useSat)
            {
                const float in = x * satDrive + satBias;
                const float y  = std::tanh (in) - satTanhBias;
                const float norm = 1.0f / juce::jmax (0.15f, std::tanh (satDrive * 0.7f));
                x = y * norm * satOutLin;
            }

            data[i] = x;
        }

        if (useComp)
            grDb.store (currentGrDb, std::memory_order_relaxed);
        else
            grDb.store (0.0f, std::memory_order_relaxed);
    }

private:
    // ------------------------------------------------------------------
    // Biquad RBJ (Cookbook) con estado direct-form-1
    // ------------------------------------------------------------------
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1 = 0, z2 = 0;

        float processSample (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void setLowShelf (double fs, double freq, double gainDb, double /*S*/)
        {
            const double A  = std::pow (10.0, gainDb / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * freq / fs;
            const double cw = std::cos (w0), sw = std::sin (w0);
            const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / 0.9 - 1.0) + 2.0);
            const double sq = 2.0 * std::sqrt (A) * alpha;

            const double b0 =    A * ((A + 1.0) - (A - 1.0) * cw + sq);
            const double b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            const double b2 =    A * ((A + 1.0) - (A - 1.0) * cw - sq);
            const double a0 =       (A + 1.0) + (A - 1.0) * cw + sq;
            const double a1 =   -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            const double a2 =       (A + 1.0) + (A - 1.0) * cw - sq;

            setFromRaw (b0, b1, b2, a0, a1, a2);
        }

        void setHighShelf (double fs, double freq, double gainDb, double /*S*/)
        {
            const double A  = std::pow (10.0, gainDb / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * freq / fs;
            const double cw = std::cos (w0), sw = std::sin (w0);
            const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / 0.9 - 1.0) + 2.0);
            const double sq = 2.0 * std::sqrt (A) * alpha;

            const double b0 =    A * ((A + 1.0) + (A - 1.0) * cw + sq);
            const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            const double b2 =    A * ((A + 1.0) + (A - 1.0) * cw - sq);
            const double a0 =       (A + 1.0) - (A - 1.0) * cw + sq;
            const double a1 =    2.0 * ((A - 1.0) - (A + 1.0) * cw);
            const double a2 =       (A + 1.0) - (A - 1.0) * cw - sq;

            setFromRaw (b0, b1, b2, a0, a1, a2);
        }

        void setPeaking (double fs, double freq, double gainDb, double Q)
        {
            const double A  = std::pow (10.0, gainDb / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * freq / fs;
            const double cw = std::cos (w0), sw = std::sin (w0);
            const double alpha = sw / (2.0 * Q);

            const double b0 =   1.0 + alpha * A;
            const double b1 =  -2.0 * cw;
            const double b2 =   1.0 - alpha * A;
            const double a0 =   1.0 + alpha / A;
            const double a1 =  -2.0 * cw;
            const double a2 =   1.0 - alpha / A;

            setFromRaw (b0, b1, b2, a0, a1, a2);
        }

        void setFromRaw (double b0d, double b1d, double b2d, double a0d, double a1d, double a2d)
        {
            const float inv = 1.0f / (float) a0d;
            b0 = (float) (b0d * inv);
            b1 = (float) (b1d * inv);
            b2 = (float) (b2d * inv);
            a1 = (float) (a1d * inv);
            a2 = (float) (a2d * inv);
        }
    };

    void updateEQ()
    {
        const double fs = sr;
        const double lg = params.eqLowGain.load  (std::memory_order_relaxed);
        const double lf = juce::jlimit (20.0, 500.0,  (double) params.eqLowFreq.load  (std::memory_order_relaxed));
        const double mg = params.eqMidGain.load  (std::memory_order_relaxed);
        const double mf = juce::jlimit (100.0, 12000.0, (double) params.eqMidFreq.load (std::memory_order_relaxed));
        const double mq = juce::jlimit (0.2, 8.0,  (double) params.eqMidQ.load (std::memory_order_relaxed));
        const double hg = params.eqHighGain.load (std::memory_order_relaxed);
        const double hf = juce::jlimit (1000.0, 20000.0, (double) params.eqHighFreq.load (std::memory_order_relaxed));

        filters[0].setLowShelf  (fs, lf, lg, 0.9);
        filters[1].setPeaking   (fs, mf, mg, mq);
        filters[2].setHighShelf (fs, hf, hg, 0.9);
    }

    std::array<Biquad, 3> filters;

    double sr = 48000.0;
    float  env     = 0.0f;
    float  grLin   = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProChannelProcessor)
};
