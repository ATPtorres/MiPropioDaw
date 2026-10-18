#pragma once

#include <JuceHeader.h>

// VU meter analógico con aguja física (segundo orden: resorte + amortiguamiento).
// Ballistics realistas: fn ≈ 2.5 Hz, ζ ≈ 0.75 (overshoot ~3%, settling ~300 ms).
class VUMeter : public juce::Component
{
public:
    void setLevel (float newRms, bool isActive)
    {
        targetRms = newRms;
        active = isActive;
        // No repaint aquí; el timer del MixerView llama tick() + repaint()
    }

    // Llamar desde el timer (~25 Hz). Integra la física de la aguja.
    void tick (double dtSeconds)
    {
        // Convierte RMS lineal → escala VU (0 VU = -18 dBFS)
        const float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, targetRms), -60.0f);
        const float vu = (db - (-18.0f)) * (3.0f / 21.0f); // mapea -21..+3 dB → -3..+3 VU aprox

        const float targetAngle = juce::jlimit (-1.0f, 1.0f, vu / 3.0f); // -1..1

        // Segundo orden: acc = ω²(target-angle) - 2ζω·vel
        const float omega = juce::MathConstants<float>::twoPi * 2.5f; // 2.5 Hz
        const float zeta  = 0.75f;
        const float dt    = (float) juce::jlimit (0.001, 0.1, dtSeconds);

        const float accel = omega * omega * (targetAngle - angle) - 2.0f * zeta * omega * velocity;
        velocity += accel * dt;
        angle    += velocity * dt;

        // limites duros
        angle    = juce::jlimit (-1.15f, 1.15f, angle);
        velocity = juce::jlimit (-10.0f, 10.0f, velocity);
    }

    void paint (juce::Graphics& g) override;

private:
    float targetRms  = 0.0f;
    float angle      = 0.0f;   // -1..1
    float velocity   = 0.0f;
    bool  active     = false;
};
