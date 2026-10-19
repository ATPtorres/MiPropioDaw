#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <algorithm>
#include "../core/IDs.h"

// Evento MIDI plano (hilo de audio no toca ValueTree).
struct SeqEvent
{
    float beat = 0.0f;      // inicio absoluto en pulsos
    int16_t pitch = 0;
    uint8_t type = 0;       // 0 = on, 1 = off
    uint8_t velocity = 100;
};

// Secuencia precalculada por slot (escrita en hilo de mensaje, leída en audio con seqlock).
struct SlotSequence
{
    static constexpr int kMaxEvents = 4096;

    std::array<SeqEvent, kMaxEvents> events {};
    int count = 0;
    std::atomic<uint32_t> generation { 0 };
};

namespace MidiSequence
{
    inline void sortEvents (SeqEvent* out, int n)
    {
        std::sort (out, out + n, [] (const SeqEvent& a, const SeqEvent& b)
        {
            if (a.beat != b.beat)
                return a.beat < b.beat;
            return a.type > b.type;
        });
    }

    inline int buildLoop (const juce::ValueTree& patternsTree, int patternUid,
                          SeqEvent* out, int maxEvents, float& loopBeats)
    {
        loopBeats = 4.0f;

        if (! out || maxEvents <= 0 || ! patternsTree.isValid() || patternUid < 0)
            return 0;

        juce::ValueTree pattern;
        for (int p = 0; p < patternsTree.getNumChildren(); ++p)
        {
            auto cand = patternsTree.getChild (p);
            if ((int) cand.getProperty (IDs::uid, -1) == patternUid)
            {
                pattern = cand;
                break;
            }
        }

        if (! pattern.isValid())
            return 0;

        loopBeats = juce::jmax (0.25f, (float) (double) pattern.getProperty (IDs::patternLen, 4.0));

        int n = 0;

        for (int i = 0; i < pattern.getNumChildren(); ++i)
        {
            auto note = pattern.getChild (i);
            if (note.getType() != juce::Identifier (IDs::note))
                continue;

            const float on = (float) (double) note.getProperty (IDs::start, 0.0);
            if (on < -1.0e-6f || on >= loopBeats)
                continue;

            const float len = juce::jmax (0.01f, (float) (double) note.getProperty (IDs::duration, 0.25));
            const float off = juce::jmin (on + len, loopBeats);

            if (n >= maxEvents)
                return n;

            out[n++] = { juce::jmax (0.0f, on), (int16_t) (int) note.getProperty (IDs::pitch, 60), 0,
                         (uint8_t) juce::jlimit (0, 127, (int) (127.0 * (double) note.getProperty (IDs::velocity, 0.8))) };

            if (n >= maxEvents)
                return n;

            out[n++] = { off, (int16_t) (int) note.getProperty (IDs::pitch, 60), 1, 0 };
        }

        sortEvents (out, n);
        return n;
    }

    // Construye los eventos de un slot a partir de los clips del track y las notas
    // de los patrones referenciados. Devuelve el número de eventos escritos.
    inline int build (const juce::ValueTree& tracksTree, const juce::ValueTree& patternsTree,
                      int slotIndex, SeqEvent* out, int maxEvents)
    {
        if (! out || maxEvents <= 0 || ! tracksTree.isValid())
            return 0;

        int n = 0;

        for (int t = 0; t < tracksTree.getNumChildren(); ++t)
        {
            auto track = tracksTree.getChild (t);
            if ((int) track.getProperty (IDs::mixerSlot, -1) != slotIndex)
                continue;

            for (int c = 0; c < track.getNumChildren(); ++c)
            {
                auto clip = track.getChild (c);
                if (clip.getType() != juce::Identifier (IDs::clip))
                    continue;

                const int puid = (int) clip.getProperty (IDs::patternUid, -1);
                juce::ValueTree pattern;
                for (int p = 0; p < patternsTree.getNumChildren(); ++p)
                {
                    auto cand = patternsTree.getChild (p);
                    if ((int) cand.getProperty (IDs::uid, -1) == puid)
                    {
                        pattern = cand;
                        break;
                    }
                }
                if (! pattern.isValid())
                    continue;

                const float clipStart   = (float) (double) clip.getProperty (IDs::start, 0.0);
                const float clipLength  = (float) (double) clip.getProperty (IDs::duration, 4.0);
                const float clipEnd     = clipStart + clipLength;

                for (int i = 0; i < pattern.getNumChildren(); ++i)
                {
                    auto note = pattern.getChild (i);
                    if (note.getType() != juce::Identifier (IDs::note))
                        continue;

                    const float on = clipStart + (float) (double) note.getProperty (IDs::start, 0.0);
                    if (on < clipStart - 1.0e-6f || on >= clipEnd - 1.0e-6f)
                        continue;

                    const float len = juce::jmax (0.01f, (float) (double) note.getProperty (IDs::duration, 0.25));
                    const float off = juce::jmin (on + len, clipEnd);

                    if (n >= maxEvents)
                        return n;

                    out[n++] = { on, (int16_t) (int) note.getProperty (IDs::pitch, 60), 0,
                                 (uint8_t) juce::jlimit (0, 127, (int) (127.0 * (double) note.getProperty (IDs::velocity, 0.8))) };

                    if (n >= maxEvents)
                        return n;

                    out[n++] = { off, (int16_t) (int) note.getProperty (IDs::pitch, 60), 1, 0 };
                }
            }
        }

        sortEvents (out, n);

        return n;
    }
}
