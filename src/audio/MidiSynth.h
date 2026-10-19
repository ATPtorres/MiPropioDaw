#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

// Comando de la interfaz → hilo de audio (SPSC, fijo, sin locks).
struct SynthCommand
{
    int16_t slot = -1;
    int16_t pitch = -1;
    uint8_t velocity = 100;
    uint8_t type = 0;   // 0 = note on, 1 = note off
};

// Cola de un productor (hilo de mensaje) y un consumidor (hilo de audio).
template <int Size>
class SynthCommandQueue
{
public:
    bool push (const SynthCommand& c)
    {
        const auto h = head.load (std::memory_order_relaxed);
        const auto next = (h + 1) % Size;
        if (next == tail.load (std::memory_order_acquire))
            return false;

        data[(size_t) h] = c;
        head.store (next, std::memory_order_release);
        return true;
    }

    bool pop (SynthCommand& out)
    {
        const auto t = tail.load (std::memory_order_relaxed);
        if (t == head.load (std::memory_order_acquire))
            return false;

        out = data[(size_t) t];
        tail.store ((t + 1) % Size, std::memory_order_release);
        return true;
    }

private:
    std::array<SynthCommand, Size> data {};
    std::atomic<int> head { 0 };
    std::atomic<int> tail { 0 };
};

// Sintetizador analógico polifónico (sin allocations en el hilo de audio).
class MidiSynth
{
public:
    static constexpr int kVoices = 16;

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        atkStep = 1.0f - (float) std::exp (-1.0 / (0.004 * sr));
        relCoef = (float) std::exp (-1.0 / (0.18 * sr));
        buildTable();
    }

    void noteOn (int p, float vel, int delaySamples = 0)
    {
        p = juce::jlimit (0, 127, p);

        for (auto& v : voices)
            if (v.pitch == p && v.active())
            {
                v.retrigger (incOf (p), vel, delaySamples);
                return;
            }

        for (auto& v : voices)
            if (! v.active())
            {
                v.start (p, incOf (p), vel, delaySamples, ++ageCounter);
                return;
            }

        Voice* victim = &voices[0];
        for (auto& v : voices)
            if (v.age < victim->age)
                victim = &v;

        victim->start (p, incOf (p), vel, delaySamples, ++ageCounter);
    }

    void noteOff (int p, int delaySamples = 0)
    {
        for (auto& v : voices)
            if (v.pitch == p && v.active() && ! v.releasing)
            {
                if (delaySamples <= 0)
                    v.releasing = true;
                else
                    v.offDelay = delaySamples;
            }
    }

    void allOff()
    {
        for (auto& v : voices)
            v.reset();
    }

    void render (float* dst, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float mix = 0.0f;

            for (auto& v : voices)
            {
                if (! v.active())
                    continue;

                if (v.delay > 0)
                {
                    --v.delay;
                    if (v.delay > 0)
                        continue;
                }

                if (v.offDelay > 0)
                {
                    --v.offDelay;
                    if (v.offDelay == 0)
                        v.releasing = true;
                }

                if (v.releasing)
                    v.env *= relCoef;
                else
                    v.env += (1.0f - v.env) * atkStep;

                if (v.releasing && v.env < 1.0e-4f)
                {
                    v.pitch = -1;
                    continue;
                }

                v.phase += v.inc;
                if (v.phase >= 1.0f)
                    v.phase -= 1.0f;

                const float f0 = lookup (v.phase);
                const float f1 = lookup (v.phase * 2.0f - (float) (int) (v.phase * 2.0f));

                mix += (f0 + 0.35f * f1) * v.env * v.gain;
            }

            dst[i] += juce::jlimit (-1.0f, 1.0f, mix * 0.35f);
        }
    }

    static float freqOf (int pitch)
    {
        return 440.0f * std::pow (2.0f, (float) (pitch - 69) / 12.0f);
    }

private:
    static constexpr int kTableSize = 2048;
    static inline std::array<float, kTableSize> table {};

    static void buildTable()
    {
        static bool built = false;
        if (built)
            return;
        built = true;

        for (int i = 0; i < kTableSize; ++i)
            table[(size_t) i] = std::sin (juce::MathConstants<float>::twoPi * (float) i / (float) kTableSize);
    }

    static float lookup (float phase)
    {
        const float x = phase * (float) kTableSize;
        const int i = (int) x;
        const float frac = x - (float) i;
        const int a = i & (kTableSize - 1);
        const int b = (i + 1) & (kTableSize - 1);
        return table[(size_t) a] + (table[(size_t) b] - table[(size_t) a]) * frac;
    }

    float incOf (int pitch) const
    {
        return (float) (freqOf (pitch) / sr);
    }

    struct Voice
    {
        int pitch = -1;
        int delay = 0;
        int offDelay = 0;
        int age = 0;
        bool releasing = false;
        float phase = 0.0f, inc = 0.0f, env = 0.0f, gain = 0.0f;

        bool active() const { return pitch >= 0; }

        void start (int p, float i, float vel, int d, int a)
        {
            pitch = p; inc = i; releasing = false;
            phase = 0.0f; env = 0.0f;
            gain = 0.2f + 0.8f * juce::jlimit (0.0f, 1.0f, vel);
            delay = juce::jmax (0, d);
            offDelay = 0;
            age = a;
        }

        void retrigger (float i, float vel, int d)
        {
            releasing = false;
            inc = i;
            gain = 0.2f + 0.8f * juce::jlimit (0.0f, 1.0f, vel);
            delay = juce::jmax (0, d);
            offDelay = 0;
        }

        void reset()
        {
            pitch = -1;
            releasing = false;
            env = 0.0f;
            offDelay = 0;
        }
    };

    std::array<Voice, kVoices> voices {};
    double sr = 48000.0;
    float atkStep = 0.0f, relCoef = 0.0f;
    int ageCounter = 0;
};
