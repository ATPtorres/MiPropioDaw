#pragma once

#include <JuceHeader.h>
#include "../audio/AudioEngine.h"
#include "../core/ProjectState.h"

// Crea/borra pistas con asignación automática de canal de mixer.
// Los slots del mixer NO se reutilizan durante la sesión (máx. 128 pistas),
// lo que hace que deshacer/rehacer sea siempre seguro.
class TrackList
{
public:
    TrackList (ProjectState& state, AudioEngine& engine);

    juce::ValueTree getTracks() const { return state.tracks(); }

    // Devuelve el árbol creado o ValueTree nulo si se superó el límite.
    juce::ValueTree addTrack (const juce::String& type);

    void removeTrack (const juce::ValueTree& track);
    void renameTrack (const juce::ValueTree& track, const juce::String& newName);

    int trackCount() const { return getTracks().getNumChildren(); }

private:
    int pickDefaultInput();
    juce::String nextName (const juce::String& base) const;

    ProjectState& state;
    AudioEngine& engine;

    static const juce::Array<juce::Colour>& palette();
};
