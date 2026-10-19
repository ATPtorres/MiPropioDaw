#include "AudioEngine.h"
#include "../core/IDs.h"
#include <cstring>

namespace
{
    juce::File configDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("MiPropioDaw");
    }

    // Devuelve la primera frontera de cuantización dentro de [b0, b1] o -1.
    double nextQuantizeBoundary (double b0, double b1, double q)
    {
        if (q <= 0.0)
            return b0;

        const double n = std::floor (b0 / q + 1.0e-9);
        double c = n * q;

        if (c < b0 - 1.0e-9)
            c += q;

        return (c <= b1 + 1.0e-9) ? c : -1.0;
    }
}

AudioEngine::AudioEngine()  = default;

AudioEngine::~AudioEngine()
{
    mgr.removeAudioCallback (this);

    for (auto& s : seqs)
    {
        delete s;
        s = nullptr;
    }

    for (auto& s : sessionLoops)
    {
        delete s;
        s = nullptr;
    }

    if (deviceInitialised)
    {
        if (auto xml = mgr.createStateXml())
        {
            auto dir = configDirectory();
            dir.createDirectory();
            dir.getChildFile ("audio.cfg.xml").replaceWithText (xml->toString());
        }
    }
}

// ---------------------------------------------------------------------------
// Dispositivos
// ---------------------------------------------------------------------------
bool AudioEngine::initDevice (juce::String& errorMessage)
{
    std::unique_ptr<juce::XmlElement> savedState;
    auto cfg = configDirectory().getChildFile ("audio.cfg.xml");
    if (cfg.existsAsFile())
        savedState = juce::parseXML (cfg);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.sampleRate = 48000.0;
    setup.bufferSize = 256;

    auto err = mgr.initialise (2, 2, savedState.get(), true);

    if (err.isNotEmpty())
        err = mgr.initialise (0, 2, savedState.get(), true);

    deviceInitialised = err.isEmpty();
    lastDeviceError   = err;
    errorMessage      = err;

    mgr.addAudioCallback (this);
    return deviceInitialised;
}

float AudioEngine::getLatencyMs() const
{
    auto* device = mgr.getCurrentAudioDevice();
    if (device == nullptr)
        return 0.0f;

    const auto sr = device->getCurrentSampleRate();
    if (sr <= 0.0)
        return 0.0f;

    const auto samples = device->getCurrentBufferSizeSamples() + device->getOutputLatencyInSamples();
    return (float) ((samples * 1000.0) / sr);
}

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------
void AudioEngine::attachTracks (const juce::ValueTree& tree)
{
    tracksTree.removeListener (this);
    tracksTree = tree;

    if (! tracksTree.isValid())
        return;

    tracksTree.addListener (this);

    for (int i = 0; i < tracksTree.getNumChildren(); ++i)
        activateFromTree (tracksTree.getChild (i));
}

void AudioEngine::attachPatterns (const juce::ValueTree& tree)
{
    patternsTree.removeListener (this);
    patternsTree = tree;

    if (! patternsTree.isValid())
        return;

    patternsTree.addListener (this);
    rebuildAllSequences();
}

void AudioEngine::attachTransport (const juce::ValueTree& tree)
{
    transportTree.removeListener (this);
    transportTree = tree;

    if (! transportTree.isValid())
        return;

    transportTree.addListener (this);
    syncTransport();
}

void AudioEngine::attachMixer (const juce::ValueTree& tree)
{
    mixerTree.removeListener (this);
    mixerTree = tree;

    if (! mixerTree.isValid())
        return;

    mixerTree.addListener (this);

    masterGainLin.store (juce::Decibels::decibelsToGain ((float) mixerTree.getProperty (IDs::masterGain, 0.0f)),
                         std::memory_order_relaxed);
}

int AudioEngine::reserveSlot()
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        bool expected = false;
        if (slots[(size_t) i].reserved.compare_exchange_strong (expected, true))
            return i;
    }
    return -1;
}

int AudioEngine::slotIndexFromTree (const juce::ValueTree& trackTree)
{
    return (int) trackTree.getProperty (IDs::mixerSlot, -1);
}

void AudioEngine::activateFromTree (const juce::ValueTree& trackTree)
{
    auto index = slotIndexFromTree (trackTree);
    if (juce::isPositiveAndBelow (index, kMaxSlots))
    {
        auto& slot = slots[(size_t) index];
        slot.reserved.store (true, std::memory_order_relaxed);

        const bool isMidiTrack = trackTree.getProperty (IDs::type).toString() == "midi";
        slot.isMidi.store (isMidiTrack, std::memory_order_relaxed);

        if (isMidiTrack && seqs[(size_t) index] == nullptr)
            seqs[(size_t) index] = new SlotSequence();

        if (isMidiTrack && sessionLoops[(size_t) index] == nullptr)
            sessionLoops[(size_t) index] = new SessionLoop();

        syncSlotFromTree (trackTree);
        syncProChannelFromTree (trackTree);
        rebuildSequence (index);
        slot.active.store (true, std::memory_order_release);
    }
}

void AudioEngine::syncSlotFromTree (const juce::ValueTree& trackTree)
{
    auto index = slotIndexFromTree (trackTree);
    if (! juce::isPositiveAndBelow (index, kMaxSlots))
        return;

    auto& slot = slots[(size_t) index];

    slot.inputChannel.store ((int) trackTree.getProperty (IDs::inputChannel, kNoInput), std::memory_order_relaxed);
    slot.gain.store (juce::Decibels::decibelsToGain ((float) trackTree.getProperty (IDs::gain, 0.0f)),
                     std::memory_order_relaxed);
    slot.pan.store (juce::jlimit (-1.0f, 1.0f, (float) trackTree.getProperty (IDs::pan, 0.0f)),
                    std::memory_order_relaxed);
    slot.mute.store ((bool) trackTree.getProperty (IDs::mute, false), std::memory_order_relaxed);
    slot.solo.store ((bool) trackTree.getProperty (IDs::solo, false), std::memory_order_relaxed);
}

void AudioEngine::syncProChannelFromTree (const juce::ValueTree& trackTree)
{
    auto index = slotIndexFromTree (trackTree);
    if (! juce::isPositiveAndBelow (index, kMaxSlots))
        return;

    auto pc = trackTree.getChildWithName (IDs::proChannel);
    if (! pc.isValid())
        return;

    auto& p = slots[(size_t) index].proChannel.params;

    p.eqEnabled.store   ((bool) pc.getProperty (IDs::pcEqEnabled, false), std::memory_order_relaxed);
    p.eqLowGain.store   ((float) pc.getProperty (IDs::pcEqLowGain, 0.0f),  std::memory_order_relaxed);
    p.eqLowFreq.store   ((float) pc.getProperty (IDs::pcEqLowFreq, 80.0f), std::memory_order_relaxed);
    p.eqMidGain.store   ((float) pc.getProperty (IDs::pcEqMidGain, 0.0f),  std::memory_order_relaxed);
    p.eqMidFreq.store   ((float) pc.getProperty (IDs::pcEqMidFreq, 1000.0f), std::memory_order_relaxed);
    p.eqMidQ.store      ((float) pc.getProperty (IDs::pcEqMidQ, 0.7f),     std::memory_order_relaxed);
    p.eqHighGain.store  ((float) pc.getProperty (IDs::pcEqHighGain, 0.0f), std::memory_order_relaxed);
    p.eqHighFreq.store  ((float) pc.getProperty (IDs::pcEqHighFreq, 8000.0f), std::memory_order_relaxed);

    p.compEnabled.store   ((bool) pc.getProperty (IDs::pcCompEnabled, false),   std::memory_order_relaxed);
    p.compThreshold.store ((float) pc.getProperty (IDs::pcCompThreshold, -18.0f), std::memory_order_relaxed);
    p.compRatio.store     ((float) pc.getProperty (IDs::pcCompRatio, 4.0f),     std::memory_order_relaxed);
    p.compAttack.store    ((float) pc.getProperty (IDs::pcCompAttack, 10.0f),   std::memory_order_relaxed);
    p.compRelease.store   ((float) pc.getProperty (IDs::pcCompRelease, 100.0f), std::memory_order_relaxed);
    p.compMakeup.store    ((float) pc.getProperty (IDs::pcCompMakeup, 0.0f),    std::memory_order_relaxed);

    p.satEnabled.store ((bool) pc.getProperty (IDs::pcSatEnabled, false), std::memory_order_relaxed);
    p.satDrive.store   ((float) pc.getProperty (IDs::pcSatDrive, 0.3f),   std::memory_order_relaxed);
    p.satBias.store    ((float) pc.getProperty (IDs::pcSatBias, 0.15f),   std::memory_order_relaxed);
    p.satOutput.store  ((float) pc.getProperty (IDs::pcSatOutput, 0.0f),  std::memory_order_relaxed);
}

void AudioEngine::rebuildSequence (int slotIndex)
{
    if (! juce::isPositiveAndBelow (slotIndex, kMaxSlots))
        return;

    auto* seq = seqs[(size_t) slotIndex];
    if (seq == nullptr || ! tracksTree.isValid())
        return;

    seqScratch.resize (SlotSequence::kMaxEvents);

    const int n = MidiSequence::build (tracksTree, patternsTree, slotIndex,
                                       seqScratch.data(), SlotSequence::kMaxEvents);

    const auto g = seq->generation.load (std::memory_order_relaxed);
    seq->generation.store (g + 1, std::memory_order_release);

    seq->count = n;
    std::memcpy (seq->events.data(), seqScratch.data(), (size_t) n * sizeof (SeqEvent));

    seq->generation.store (g + 2, std::memory_order_release);
}

void AudioEngine::rebuildAllSequences()
{
    for (int i = 0; i < kMaxSlots; ++i)
        if (seqs[(size_t) i] != nullptr)
            rebuildSequence (i);
}

void AudioEngine::rebuildLoop (int slotIndex, int patternUid)
{
    if (! juce::isPositiveAndBelow (slotIndex, kMaxSlots) || patternUid < 0)
        return;

    auto* L = sessionLoops[(size_t) slotIndex];
    if (L == nullptr || ! patternsTree.isValid())
        return;

    seqScratch.resize (SessionLoop::kMaxLoopEvents);

    float loopBeats = 4.0f;
    const int n = MidiSequence::buildLoop (patternsTree, patternUid,
                                           seqScratch.data(), SessionLoop::kMaxLoopEvents, loopBeats);

    const auto g = L->generation.load (std::memory_order_relaxed);
    L->generation.store (g + 1, std::memory_order_release);

    L->count     = n;
    L->loopBeats = loopBeats;
    std::memcpy (L->events.data(), seqScratch.data(), (size_t) n * sizeof (SeqEvent));

    L->generation.store (g + 2, std::memory_order_release);
}

void AudioEngine::rebuildActiveLoops()
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        auto* L = sessionLoops[(size_t) i];
        if (L == nullptr)
            continue;

        int uid = L->pendingPattern.load (std::memory_order_relaxed);
        if (uid < 0 && L->active.load (std::memory_order_relaxed))
            uid = L->activePattern.load (std::memory_order_relaxed);

        if (uid >= 0)
            rebuildLoop (i, uid);
    }
}

void AudioEngine::releaseLoopNotes (int slotIndex, SessionLoop& loop)
{
    auto& s = slots[(size_t) juce::jlimit (0, kMaxSlots - 1, slotIndex)];

    for (int p = 0; p < 128; ++p)
    {
        if (loop.sounding[(size_t) p] != 0)
        {
            s.synth.noteOff (p);
            loop.sounding[(size_t) p] = 0;
        }
    }
}

void AudioEngine::launchSessionClip (int slot, int patternUid)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots) || patternUid < 0)
        return;

    auto* L = sessionLoops[(size_t) slot];
    if (L == nullptr)
        return;

    rebuildLoop (slot, patternUid);
    L->pendingStop.store (false, std::memory_order_relaxed);
    L->pendingPattern.store (patternUid, std::memory_order_release);
}

void AudioEngine::stopSessionClip (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots))
        return;

    auto* L = sessionLoops[(size_t) slot];
    if (L == nullptr)
        return;

    L->pendingPattern.store (-1, std::memory_order_release);
    L->pendingStop.store (true, std::memory_order_relaxed);
}

void AudioEngine::stopAllSessionClips()
{
    for (auto* L : sessionLoops)
    {
        if (L == nullptr)
            continue;

        L->pendingPattern.store (-1, std::memory_order_release);
        L->pendingStop.store (true, std::memory_order_relaxed);
    }
}

void AudioEngine::resetSessionLoops()
{
    for (auto* L : sessionLoops)
    {
        if (L == nullptr)
            continue;

        L->active.store (false, std::memory_order_relaxed);
        L->activePattern.store (-1, std::memory_order_relaxed);
        L->pendingPattern.store (-1, std::memory_order_relaxed);
        L->pendingStop.store (false, std::memory_order_relaxed);
        L->progress.store (0.0f, std::memory_order_relaxed);
        L->sounding.fill (0);
    }
}

bool AudioEngine::isSessionActive (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots) || sessionLoops[(size_t) slot] == nullptr)
        return false;

    return sessionLoops[(size_t) slot]->active.load (std::memory_order_relaxed);
}

int AudioEngine::sessionQueuedPatternFor (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots) || sessionLoops[(size_t) slot] == nullptr)
        return -1;

    return sessionLoops[(size_t) slot]->pendingPattern.load (std::memory_order_relaxed);
}

int AudioEngine::sessionPatternFor (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots) || sessionLoops[(size_t) slot] == nullptr)
        return -1;

    auto* L = sessionLoops[(size_t) slot];
    return L->active.load (std::memory_order_relaxed) ? L->activePattern.load (std::memory_order_relaxed) : -1;
}

float AudioEngine::sessionLoopProgress (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots) || sessionLoops[(size_t) slot] == nullptr)
        return 0.0f;

    return sessionLoops[(size_t) slot]->progress.load (std::memory_order_relaxed);
}

int AudioEngine::processSession (double b0, double b1, int numSamples, bool discontinuity)
{
    if (! (b1 > b0))
        return 0;

    const bool isPlayingNow = playing.load (std::memory_order_relaxed);
    const float q = sessionQuantize.load (std::memory_order_relaxed);
    int scheduled = 0;

    for (int i = 0; i < kMaxSlots; ++i)
    {
        auto* L = sessionLoops[(size_t) i];
        if (L == nullptr)
            continue;

        if (! isPlayingNow)
        {
            if (L->active.exchange (false, std::memory_order_acq_rel))
            {
                releaseLoopNotes (i, *L);
                L->activePattern.store (-1, std::memory_order_relaxed);
            }

            L->pendingStop.store (false, std::memory_order_relaxed);
            L->progress.store (0.0f, std::memory_order_relaxed);
            continue;
        }

        if (discontinuity && L->active.load (std::memory_order_relaxed))
            L->loopStartBeat = b0;

        const bool stopReq = L->pendingStop.load (std::memory_order_relaxed);
        const bool havePending = L->pendingPattern.load (std::memory_order_acquire) >= 0;

        if (stopReq || havePending)
        {
            const double at = nextQuantizeBoundary (b0, b1, (double) q);

            if (at >= 0.0)
            {
                if (stopReq)
                {
                    if (L->active.exchange (false, std::memory_order_acq_rel))
                        releaseLoopNotes (i, *L);

                    L->activePattern.store (-1, std::memory_order_relaxed);
                    L->pendingPattern.store (-1, std::memory_order_release);
                    L->pendingStop.store (false, std::memory_order_relaxed);
                    L->progress.store (0.0f, std::memory_order_relaxed);
                }
                else
                {
                    int want = -1;
                    for (int t = 0; t < 4; ++t)
                    {
                        const int cur = L->pendingPattern.load (std::memory_order_acquire);
                        if (cur < 0)
                            break;

                        int expected = cur;
                        if (L->pendingPattern.compare_exchange_strong (expected, -1,
                                                                       std::memory_order_acq_rel))
                        {
                            want = cur;
                            break;
                        }
                    }

                    if (want >= 0)
                    {
                        if (L->active.exchange (false, std::memory_order_acq_rel))
                            releaseLoopNotes (i, *L);

                        L->loopStartBeat = at;
                        L->activePattern.store (want, std::memory_order_relaxed);
                        L->sounding.fill (0);
                        L->active.store (true, std::memory_order_release);
                    }
                }
            }
        }

        if (! L->active.load (std::memory_order_acquire))
            continue;

        {
            const double lb0 = (double) L->loopBeats;
            if (lb0 > 1.0e-4)
            {
                const double d = (b0 - L->loopStartBeat) / lb0;
                const double frac = d - std::floor (d);
                L->progress.store ((float) frac, std::memory_order_relaxed);
            }
        }

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto g1 = L->generation.load (std::memory_order_acquire);
            if ((g1 & 1u) != 0)
                continue;

            const int count   = L->count;
            const float loopB = L->loopBeats;

            struct Segment { double start, end, base; };
            Segment segs[64];
            int segCount = 0;
            double iterBase = L->loopStartBeat;
            double cursor   = juce::jmax (b0, iterBase);

            if (loopB > 1.0e-4f)
            {
                while (cursor < b1 && segCount < 64)
                {
                    const double iterEnd = iterBase + (double) loopB;
                    const double segEnd  = juce::jmin (b1, iterEnd);

                    if (segEnd > cursor)
                        segs[segCount++] = { cursor, segEnd, iterBase };

                    cursor   = segEnd;
                    iterBase = iterEnd;
                }
            }

            const auto g2 = L->generation.load (std::memory_order_acquire);
            if (g1 != g2 || (g2 & 1u) != 0)
                continue;

            auto& s = slots[(size_t) i];

            for (int si = 0; si < segCount; ++si)
            {
                const auto& seg = segs[si];

                for (int k = 0; k < count; ++k)
                {
                    const auto& e = L->events[(size_t) k];
                    const double absBeat = seg.base + (double) e.beat;

                    if (absBeat < seg.start || absBeat >= seg.end)
                        continue;

                    const double frac = (b1 > b0) ? (absBeat - b0) / (b1 - b0) : 0.0;
                    const int ns = juce::jmax (1, numSamples);
                    const int delay = juce::jlimit (0, ns - 1, (int) (frac * (double) ns));
                    const int pitch = juce::jlimit (0, 127, (int) e.pitch);

                    if (e.type == 0)
                    {
                        s.synth.noteOn (pitch, (float) e.velocity / 127.0f, delay);
                        L->sounding[(size_t) pitch] = 1;
                    }
                    else
                    {
                        s.synth.noteOff (pitch, delay);
                        L->sounding[(size_t) pitch] = 0;
                    }

                    ++scheduled;
                }
            }

            L->loopStartBeat = iterBase;
            break;
        }
    }

    return scheduled;
}

void AudioEngine::collectEvents (int slotIndex, float beatStart, float beatEnd,
                                 SeqEvent* out, int maxOut, int& outCount)
{
    outCount = 0;

    if (! juce::isPositiveAndBelow (slotIndex, kMaxSlots))
        return;

    auto* seq = seqs[(size_t) slotIndex];
    if (seq == nullptr)
        return;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto g1 = seq->generation.load (std::memory_order_acquire);
        if ((g1 & 1u) != 0)
            continue;

        const int count = seq->count;
        outCount = 0;

        for (int i = 0; i < count && outCount < maxOut; ++i)
        {
            const auto& e = seq->events[(size_t) i];
            if (e.beat >= beatStart && e.beat < beatEnd)
                out[outCount++] = e;
        }

        const auto g2 = seq->generation.load (std::memory_order_acquire);
        if (g1 == g2 && (g2 & 1u) == 0)
            return;
    }

    outCount = 0;
}

void AudioEngine::previewNoteOn (int slot, int pitch, float velocity01)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots))
        return;

    synthQueue.push ({ (int16_t) slot, (int16_t) pitch,
                       (uint8_t) juce::jlimit (1, 127, (int) (127.0f * juce::jlimit (0.0f, 1.0f, velocity01))),
                       0 });
}

void AudioEngine::previewNoteOff (int slot, int pitch)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots))
        return;

    synthQueue.push ({ (int16_t) slot, (int16_t) pitch, 0, 1 });
}

void AudioEngine::stop()
{
    playing.store (false, std::memory_order_relaxed);
    transportResetFlag.store (true, std::memory_order_relaxed);
    resetSessionLoops();
}

void AudioEngine::syncTransport()
{
    if (! transportTree.isValid())
        return;

    bpm.store ((double) transportTree.getProperty (IDs::bpm, 140.0), std::memory_order_relaxed);
    timeSigNum.store ((int) transportTree.getProperty (IDs::timeSigNum, 4), std::memory_order_relaxed);
    metronomeOn.store ((bool) transportTree.getProperty (IDs::metronome, false), std::memory_order_relaxed);
    recordArmed.store ((bool) transportTree.getProperty (IDs::record, false), std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// ValueTree -> slots (hilo de mensaje)
// ---------------------------------------------------------------------------
void AudioEngine::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    rebuildActiveLoops();

    if (parent == tracksTree)
    {
        activateFromTree (child);
        rebuildAllSequences();
    }
    else if (tracksTree.isValid() && parent.getParent() == tracksTree)
    {
        rebuildAllSequences();
    }
    else if (patternsTree.isValid() && parent.getParent() == patternsTree)
    {
        rebuildAllSequences();
    }
}

void AudioEngine::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index)
{
    juce::ignoreUnused (index);

    rebuildActiveLoops();

    if (parent == tracksTree)
    {
        auto slot = slotIndexFromTree (child);
        if (juce::isPositiveAndBelow (slot, kMaxSlots))
        {
            slots[(size_t) slot].active.store (false, std::memory_order_release);

            if (auto* L = sessionLoops[(size_t) slot]; L != nullptr)
            {
                L->pendingPattern.store (-1, std::memory_order_relaxed);
                L->pendingStop.store (true, std::memory_order_relaxed);
            }
        }

        rebuildAllSequences();
    }
    else if (tracksTree.isValid() && parent.getParent() == tracksTree)
    {
        rebuildAllSequences();
    }
    else if (patternsTree.isValid() && parent.getParent() == patternsTree)
    {
        rebuildAllSequences();
    }
}

void AudioEngine::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    juce::ignoreUnused (prop);

    rebuildActiveLoops();

    if (tracksTree.isValid() && tree.getParent() == tracksTree)
    {
        syncSlotFromTree (tree);
    }
    else if (tracksTree.isValid() && tree.getType() == juce::Identifier (IDs::proChannel)
             && tree.getParent().getParent() == tracksTree)
    {
        syncProChannelFromTree (tree.getParent());
    }
    else if (transportTree.isValid() && tree == transportTree)
    {
        syncTransport();
    }
    else if (mixerTree.isValid() && tree == mixerTree)
    {
        masterGainLin.store (juce::Decibels::decibelsToGain ((float) mixerTree.getProperty (IDs::masterGain, 0.0f)),
                             std::memory_order_relaxed);
    }
}

// ---------------------------------------------------------------------------
// Transporte
// ---------------------------------------------------------------------------
double AudioEngine::positionBeats() const
{
    const auto sr  = sampleRate.load (std::memory_order_relaxed);
    const auto b   = bpm.load (std::memory_order_relaxed);
    const auto pos = (double) playheadSamples.load (std::memory_order_relaxed);

    if (sr <= 0.0 || b <= 0.0)
        return 0.0;

    return pos / sr * (b / 60.0);
}

void AudioEngine::setPlayheadBeats (double beats)
{
    const auto sr = sampleRate.load (std::memory_order_relaxed);
    const auto b  = bpm.load (std::memory_order_relaxed);
    if (sr <= 0.0 || b <= 0.0)
        return;

    playheadSamples.store ((int64_t) juce::jmax (0.0, beats * 60.0 / b * sr), std::memory_order_relaxed);
    lastBeat.store (std::numeric_limits<int>::min(), std::memory_order_relaxed);
    transportResetFlag.store (true, std::memory_order_relaxed);
}

juce::String AudioEngine::positionText() const
{
    const auto beats = juce::jmax (0.0, positionBeats());
    const auto sig   = juce::jmax (1, timeSigNum.load (std::memory_order_relaxed));

    const auto barIndex  = (int) (beats / sig);
    const auto inBar     = beats - (double) barIndex * sig;
    const auto beatIndex = (int) inBar;
    const auto tick      = (int) ((inBar - beatIndex) * 960.0);

    return juce::String::formatted ("%03d.%d.%03d", barIndex + 1, beatIndex + 1, tick);
}

// ---------------------------------------------------------------------------
// Callback de audio — SIN allocations, SIN locks
// ---------------------------------------------------------------------------
void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (context);

    const auto t0 = std::chrono::steady_clock::now();
    const auto sr = sampleRate.load (std::memory_order_relaxed);

    for (int c = 0; c < numOutputChannels; ++c)
        juce::FloatVectorOperations::clear (outputChannelData[c], numSamples);

    const bool validSize = numSamples > 0 && (size_t) numSamples <= scratch.size();

    if (numOutputChannels > 0 && validSize)
    {
        const auto posAtStart = playheadSamples.load (std::memory_order_relaxed);
        const auto isPlaying  = playing.load (std::memory_order_relaxed);
        const auto bpmNow     = bpm.load (std::memory_order_relaxed);

        // --- comandos de preview y reset del transporte ----------------
        {
            SynthCommand cmd;
            while (synthQueue.pop (cmd))
            {
                auto& s = slots[(size_t) juce::jlimit (0, kMaxSlots - 1, (int) cmd.slot)];

                if (cmd.type == 0)
                    s.synth.noteOn (cmd.pitch, (float) cmd.velocity / 127.0f);
                else
                    s.synth.noteOff (cmd.pitch);
            }

            if (transportResetFlag.exchange (false, std::memory_order_relaxed))
            {
                for (auto& s : slots)
                    s.synth.allOff();

                prevEndBeat = -1.0;
            }
        }

        // --- eventos MIDI de los clips del arreglo ---------------------
        const double b0 = (double) posAtStart / sr * (bpmNow / 60.0);
        const double b1 = (double) (posAtStart + numSamples) / sr * (bpmNow / 60.0);
        const double blockBeats = b1 - b0;
        const bool discontinuity = prevEndBeat < 0.0 || std::abs (b0 - prevEndBeat) > 1.0e-4;

        if (isPlaying)
        {
            if (discontinuity)
                for (auto& s : slots)
                    s.synth.allOff();

            SeqEvent local[256];
            int nLocal = 0;

            for (int slotIdx = 0; slotIdx < kMaxSlots; ++slotIdx)
            {
                if (! slots[(size_t) slotIdx].active.load (std::memory_order_relaxed)
                    || seqs[(size_t) slotIdx] == nullptr)
                    continue;

                collectEvents (slotIdx, (float) b0, (float) b1, local, 256, nLocal);

                for (int k = 0; k < nLocal; ++k)
                {
                    const auto& e = local[k];
                    const double frac = blockBeats > 0.0 ? (e.beat - b0) / blockBeats : 0.0;
                    const int delay = juce::jlimit (0, numSamples - 1, (int) (frac * (double) numSamples));

                    if (e.type == 0)
                        slots[(size_t) slotIdx].synth.noteOn (e.pitch, (float) e.velocity / 127.0f, delay);
                    else
                        slots[(size_t) slotIdx].synth.noteOff (e.pitch, delay);
                }
            }

            prevEndBeat = b1;
        }
        else
        {
            prevEndBeat = -1.0;
        }

        processSession (b0, b1, numSamples, discontinuity);

        // --- ¿alguna pista en solo? ---------------------------------
        bool anySolo = false;
        for (const auto& s : slots)
        {
            if (s.active.load (std::memory_order_relaxed) && s.solo.load (std::memory_order_relaxed))
            {
                anySolo = true;
                break;
            }
        }

        // --- mezcla por canal ---------------------------------------
        float* dst = scratch.data();

        for (auto& s : slots)
        {
            if (! s.active.load (std::memory_order_acquire))
            {
                s.peak.store (0.0f, std::memory_order_relaxed);
                s.rms.store  (0.0f, std::memory_order_relaxed);
                continue;
            }

            const int src = s.inputChannel.load (std::memory_order_relaxed);
            bool haveSource = false;

            if (src >= 0 && src < numInputChannels && inputChannelData != nullptr && inputChannelData[src] != nullptr)
            {
                juce::FloatVectorOperations::copy (dst, inputChannelData[src], numSamples);
                haveSource = true;
            }
            else if (src == kTestToneInput)
            {
                const float inc = (float) (440.0 / sr);
                auto phase = s.tonePhase;
                for (int i = 0; i < numSamples; ++i)
                {
                    phase += inc;
                    if (phase >= 1.0f) phase -= 1.0f;
                    dst[i] = std::sin (phase * juce::MathConstants<float>::twoPi) * 0.25f;
                }
                s.tonePhase = phase;
                haveSource = true;
            }

            if (! haveSource && s.isMidi.load (std::memory_order_relaxed))
            {
                s.synth.render (dst, numSamples);
                haveSource = true;
            }

            if (! haveSource)
            {
                s.peak.store (0.0f, std::memory_order_relaxed);
                s.rms.store  (0.0f, std::memory_order_relaxed);
                continue;
            }

            // --- ProChannel DSP (pre-fader) ---------------------------
            s.proChannel.process (dst, numSamples);

            const bool gated = ! s.mute.load (std::memory_order_relaxed)
                            && ! (anySolo && ! s.solo.load (std::memory_order_relaxed));

            const float g = gated ? s.gain.load (std::memory_order_relaxed) : 0.0f;

            if (g > 0.0f)
            {
                const float pan = juce::jlimit (-1.0f, 1.0f, s.pan.load (std::memory_order_relaxed));
                const float angle = (pan + 1.0f) * juce::MathConstants<float>::halfPi * 0.5f;
                const float gl = std::cos (angle);
                const float gr = std::sin (angle);

                juce::FloatVectorOperations::add (outputChannelData[0], dst, g * gl, numSamples);
                if (numOutputChannels > 1)
                    juce::FloatVectorOperations::add (outputChannelData[1], dst, g * gr, numSamples);
            }

            // medidor del canal (post-fader, pre-paneo)
            float maxAbs = 0.0f;
            double sumSq = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                const float v = dst[i] * g;
                const float a = std::abs (v);
                if (a > maxAbs) maxAbs = a;
                sumSq += (double) v * (double) v;
            }

            const float newRms = (float) std::sqrt (sumSq / (double) numSamples);
            const float prevPeak = s.peak.load (std::memory_order_relaxed);
            s.peak.store (std::max (maxAbs, prevPeak * 0.85f), std::memory_order_relaxed);

            const float prevRms = s.rms.load (std::memory_order_relaxed);
            s.rms.store (prevRms * 0.7f + newRms * 0.3f, std::memory_order_relaxed);
        }

        // --- master -------------------------------------------------
        const float mg = masterGainLin.load (std::memory_order_relaxed);
        juce::FloatVectorOperations::multiply (outputChannelData[0], outputChannelData[0], mg, numSamples);
        if (numOutputChannels > 1)
            juce::FloatVectorOperations::multiply (outputChannelData[1], outputChannelData[1], mg, numSamples);

        // spectrum FFT del master (canal 0 mono)
        spectrum.push (outputChannelData[0], numSamples);

        // --- metrónomo ----------------------------------------------
        if (isPlaying && metronomeOn.load (std::memory_order_relaxed))
        {
            const double beatsAtStart = (double) posAtStart / sr * (bpm.load (std::memory_order_relaxed) / 60.0);
            const int curBeat = (int) std::floor (beatsAtStart + 1.0e-9);

            if (curBeat != lastBeat.load (std::memory_order_relaxed))
            {
                lastBeat.store (curBeat, std::memory_order_relaxed);
                clickEnv  = 1.0f;
                const auto sig = juce::jmax (1, timeSigNum.load (std::memory_order_relaxed));
                clickFreq = (curBeat % sig == 0) ? 1600.0 : 1100.0;
            }
        }

        if (clickEnv > 1.0e-4f)
        {
            const float decay = std::exp (-1.0f / (float) (0.025 * sr));
            const float inc   = (float) (clickFreq / sr);

            for (int i = 0; i < numSamples; ++i)
            {
                clickPhase += inc;
                if (clickPhase >= 1.0f) clickPhase -= 1.0f;

                const float v = (clickPhase < 0.5f ? 1.0f : -1.0f) * clickEnv * 0.2f;
                outputChannelData[0][i] += v;
                if (numOutputChannels > 1)
                    outputChannelData[1][i] += v;

                clickEnv *= decay;
            }
        }
        else
        {
            clickEnv = 0.0f;
        }

        // --- medidor master -----------------------------------------
        {
            float maxAbs = 0.0f;
            double sumSq = 0.0;
            const int n = numSamples;

            for (int i = 0; i < n; ++i)
            {
                const float a = std::abs (outputChannelData[0][i]);
                if (a > maxAbs) maxAbs = a;
                sumSq += (double) a * (double) a;
            }
            if (numOutputChannels > 1)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float a = std::abs (outputChannelData[1][i]);
                    if (a > maxAbs) maxAbs = a;
                    sumSq += (double) a * (double) a;
                }
            }

            const float newRms = (float) std::sqrt (sumSq / (double) (n * juce::jmax (1, juce::jmin (numOutputChannels, 2))));
            const float prevPeak = masterPeak.load (std::memory_order_relaxed);
            masterPeak.store (std::max (maxAbs, prevPeak * 0.85f), std::memory_order_relaxed);

            const float prevRms = masterRms.load (std::memory_order_relaxed);
            masterRms.store (prevRms * 0.7f + newRms * 0.3f, std::memory_order_relaxed);
        }

        // --- transporte ---------------------------------------------
        if (isPlaying)
            playheadSamples.store (posAtStart + numSamples, std::memory_order_relaxed);
    }

    // --- CPU --------------------------------------------------------
    const auto dt = std::chrono::duration<float> (std::chrono::steady_clock::now() - t0).count();
    const float blockSec = sr > 0.0 ? (float) numSamples / (float) sr : 0.0f;
    const float loadPct  = blockSec > 0.0f ? (dt / blockSec) * 100.0f : 0.0f;

    const float prevCpu = cpu.load (std::memory_order_relaxed);
    cpu.store (prevCpu * 0.9f + loadPct * 0.1f, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    sampleRate.store (device->getCurrentSampleRate(), std::memory_order_relaxed);
    blockSizeSamples.store (device->getCurrentBufferSizeSamples(), std::memory_order_relaxed);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    mgr.getAudioDeviceSetup (setup);
    deviceInputs.store (setup.useDefaultInputChannels
                            ? device->getActiveInputChannels().countNumberOfSetBits()
                            : setup.inputChannels.countNumberOfSetBits(),
                        std::memory_order_relaxed);

    const auto wanted = (size_t) juce::jmax (4096, device->getCurrentBufferSizeSamples() * 4);
    scratch.resize (wanted);

    // Prepara todos los ProChannel processors y sintetizadores
    for (auto& s : slots)
    {
        s.proChannel.prepare (sampleRate.load (std::memory_order_relaxed),
                              device->getCurrentBufferSizeSamples());
        s.synth.prepare (sampleRate.load (std::memory_order_relaxed));
    }

    spectrum.prepare (sampleRate.load (std::memory_order_relaxed));

    lastBeat.store (std::numeric_limits<int>::min(), std::memory_order_relaxed);
    clickEnv   = 0.0f;
    clickPhase = 0.0f;
}

void AudioEngine::audioDeviceStopped()
{
    scratch.clear();
    playing.store (false, std::memory_order_relaxed);
    resetSessionLoops();
}
