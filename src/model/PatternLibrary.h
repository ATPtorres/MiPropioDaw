#pragma once

#include <JuceHeader.h>
#include "../core/ProjectState.h"

// Lógica de patrones, notas y clips de arreglo.
// Sin estado propio: trabaja sobre el ValueTree del proyecto.
class PatternLibrary
{
public:
    explicit PatternLibrary (ProjectState& stateToUse) : state (stateToUse) {}

    juce::ValueTree patterns() const { return state.patterns(); }
    juce::ValueTree tracks()   const { return state.tracks(); }

    juce::ValueTree addPattern (const juce::String& name = {}, int numBars = 4);
    juce::ValueTree duplicatePattern (const juce::ValueTree& pattern);
    bool deletePattern (const juce::ValueTree& pattern);
    juce::ValueTree patternForUid (int uid) const;
    int uidOf (const juce::ValueTree& pattern) const;
    bool patternInUse (int uid) const;
    int patternCount() const { return patterns().getNumChildren(); }
    double patternLengthBeats (const juce::ValueTree& pattern) const;

    int activePatternUid() const;
    juce::ValueTree activePattern() const;
    void setActivePatternUid (int uid);

    juce::ValueTree addNote (const juce::ValueTree& pattern, int pitch, double pos,
                             double len, double vel, bool newTransaction = true);
    bool deleteNote (const juce::ValueTree& pattern, const juce::ValueTree& note);
    juce::ValueTree noteAt (const juce::ValueTree& pattern, int pitch, double pos) const;
    int noteCount (const juce::ValueTree& pattern) const;

    juce::ValueTree addClip (const juce::ValueTree& track, int patternUid, double start, double length);
    bool deleteClip (const juce::ValueTree& track, const juce::ValueTree& clip);
    bool makeUnique (const juce::ValueTree& clip);
    int clipCount() const;
    juce::ValueTree clipForUid (int uid) const;

    juce::ValueTree session() const { return state.session(); }

    int sceneCount() const;
    juce::ValueTree sceneAt (int index) const;
    juce::ValueTree sceneForUid (int uid) const;
    int sceneIndexForUid (int uid) const;
    juce::ValueTree addScene (const juce::String& name = {});
    bool deleteScene (const juce::ValueTree& scene);
    void renameScene (const juce::ValueTree& scene, const juce::String& newName);

    juce::ValueTree addSessionClip (const juce::ValueTree& track, int sceneUid, int patternUid);
    bool deleteSessionClip (const juce::ValueTree& track, const juce::ValueTree& clip);
    juce::ValueTree sessionClipAt (const juce::ValueTree& track, int sceneUid) const;
    juce::ValueTree sessionClipForUid (int uid) const;
    int sessionClipCount() const;

    float quantizeBeats() const;
    void setQuantizeBeats (float beats);

    juce::String viewMode() const;
    void setViewMode (const juce::String& mode);

    int allocateUid();

    static juce::String uniqueName (const juce::ValueTree& patternsTree, const juce::String& base);

private:
    ProjectState& state;
};
