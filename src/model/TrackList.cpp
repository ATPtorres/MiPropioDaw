#include "TrackList.h"
#include "../core/IDs.h"

namespace
{
    constexpr const char* kAudio = "audio";
    constexpr const char* kMidi  = "midi";
}

TrackList::TrackList (ProjectState& stateToUse, AudioEngine& engineToUse)
    : state (stateToUse), engine (engineToUse)
{
}

juce::ValueTree TrackList::addTrack (const juce::String& type)
{
    const bool isAudio = (type == kAudio);

    const int slot = engine.reserveSlot();
    if (slot < 0)
        return {};

    juce::ValueTree tree (IDs::track);
    tree.setProperty (IDs::type,        isAudio ? kAudio : kMidi, nullptr);
    tree.setProperty (IDs::name,        nextName (isAudio ? "Audio" : "MIDI"), nullptr);
    tree.setProperty (IDs::colour,      (int) palette()[trackCount() % palette().size()].getARGB(), nullptr);
    tree.setProperty (IDs::mixerSlot,   slot, nullptr);
    tree.setProperty (IDs::inputChannel, isAudio ? pickDefaultInput() : AudioEngine::kNoInput, nullptr);
    tree.setProperty (IDs::gain,        0.0f, nullptr);
    tree.setProperty (IDs::pan,         0.0f, nullptr);
    tree.setProperty (IDs::mute,        false, nullptr);
    tree.setProperty (IDs::solo,        false, nullptr);

    // ProChannel por defecto (todo apagado)
    juce::ValueTree pc (IDs::proChannel);
    pc.setProperty (IDs::pcEqEnabled,   false, nullptr);
    pc.setProperty (IDs::pcEqLowGain,   0.0f,  nullptr);
    pc.setProperty (IDs::pcEqLowFreq,   80.0f, nullptr);
    pc.setProperty (IDs::pcEqMidGain,   0.0f,  nullptr);
    pc.setProperty (IDs::pcEqMidFreq,   1000.0f, nullptr);
    pc.setProperty (IDs::pcEqMidQ,      0.7f,  nullptr);
    pc.setProperty (IDs::pcEqHighGain,  0.0f,  nullptr);
    pc.setProperty (IDs::pcEqHighFreq,  8000.0f, nullptr);
    pc.setProperty (IDs::pcCompEnabled,   false, nullptr);
    pc.setProperty (IDs::pcCompThreshold, -18.0f, nullptr);
    pc.setProperty (IDs::pcCompRatio,     4.0f,  nullptr);
    pc.setProperty (IDs::pcCompAttack,    10.0f, nullptr);
    pc.setProperty (IDs::pcCompRelease,   100.0f, nullptr);
    pc.setProperty (IDs::pcCompMakeup,    0.0f,  nullptr);
    pc.setProperty (IDs::pcSatEnabled, false, nullptr);
    pc.setProperty (IDs::pcSatDrive,   0.3f,  nullptr);
    pc.setProperty (IDs::pcSatBias,    0.15f, nullptr);
    pc.setProperty (IDs::pcSatOutput,  0.0f,  nullptr);
    tree.addChild (pc, -1, nullptr);

    state.addChild (getTracks(), tree, -1,
                    isAudio ? "Add audio track" : "Add MIDI track");

    return getTracks().getChildWithProperty (IDs::mixerSlot, slot);
}

void TrackList::removeTrack (const juce::ValueTree& track)
{
    if (track.isValid() && track.getParent() == getTracks())
        state.removeChild (getTracks(), track, "Delete track");
}

void TrackList::renameTrack (const juce::ValueTree& track, const juce::String& newName)
{
    if (newName.isNotEmpty())
        state.setProperty (track, IDs::name, newName, "Rename track");
}

int TrackList::pickDefaultInput()
{
    const int n = engine.numDeviceInputs();

    // Sin entradas de dispositivo: tono de prueba para poder oír el canal.
    if (n == 0)
        return AudioEngine::kTestToneInput;

    juce::Array<int> used;
    auto tracks = getTracks();
    for (int i = 0; i < tracks.getNumChildren(); ++i)
    {
        auto child = tracks.getChild (i);
        if (child.getProperty (IDs::type).toString() == kAudio)
            used.add ((int) child.getProperty (IDs::inputChannel, AudioEngine::kNoInput));
    }

    for (int i = 0; i < n; ++i)
        if (! used.contains (i))
            return i;

    return AudioEngine::kNoInput; // todas las entradas ocupadas
}

juce::String TrackList::nextName (const juce::String& base) const
{
    auto tracks = getTracks();

    for (int n = 1; n < 10000; ++n)
    {
        auto candidate = base + " " + juce::String (n);
        bool found = false;

        for (int i = 0; i < tracks.getNumChildren(); ++i)
        {
            if (tracks.getChild (i).getProperty (IDs::name).toString() == candidate)
            {
                found = true;
                break;
            }
        }

        if (! found)
            return candidate;
    }

    return base;
}

const juce::Array<juce::Colour>& TrackList::palette()
{
    static const juce::Array<juce::Colour> colours {
        juce::Colour (0xffe57373), juce::Colour (0xfff06292), juce::Colour (0xffba68c8),
        juce::Colour (0xff64b5f6), juce::Colour (0xff4dd0e1), juce::Colour (0xff81c784),
        juce::Colour (0xffffd54f), juce::Colour (0xffff8a65), juce::Colour (0xffa1887f),
        juce::Colour (0xff90a4ae)
    };
    return colours;
}
