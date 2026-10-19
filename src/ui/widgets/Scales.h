#pragma once

#include <JuceHeader.h>

// Escalas musculares para el resaltado del piano roll.
namespace Scales
{
    inline const std::vector<std::pair<juce::String, juce::Array<int>>>& all()
    {
        static const std::vector<std::pair<juce::String, juce::Array<int>>> table {
            { "Off",     {} },
            { "Major",   { 0, 2, 4, 5, 7, 9, 11 } },
            { "Minor",   { 0, 2, 3, 5, 7, 8, 10 } },
            { "Dorian",  { 0, 2, 3, 5, 7, 9, 10 } },
            { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
            { "Pentatonic", { 0, 2, 4, 7, 9 } },
            { "Minor Pentatonic", { 0, 3, 5, 7, 10 } },
            { "Blues",   { 0, 3, 5, 6, 7, 10 } },
            { "Chromatic", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }
        };
        return table;
    }

    inline juce::StringArray names()
    {
        juce::StringArray out;
        for (const auto& e : all())
            out.add (e.first);
        return out;
    }

    inline juce::Array<int> intervalsFor (const juce::String& name)
    {
        for (const auto& e : all())
            if (e.first == name)
                return e.second;
        return {};
    }

    inline bool inScale (int pitch, int root, const juce::Array<int>& intervals)
    {
        if (intervals.isEmpty())
            return true;

        const int pc = ((pitch - root) % 12 + 12) % 12;
        return intervals.contains (pc);
    }

    inline juce::String noteName (int pitch)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int n = juce::jlimit (0, 127, pitch);
        return juce::String (names[n % 12]) + juce::String ((n / 12) - 1);
    }
}
