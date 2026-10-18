#pragma once

#include <JuceHeader.h>
#include "IDs.h"

// Fuente única de verdad del proyecto: un ValueTree serializable + UndoManager.
class ProjectState
{
public:
    ProjectState();

    juce::ValueTree root;
    juce::UndoManager undo;

    juce::ValueTree transport() const { return root.getChildWithName (IDs::transport); }
    juce::ValueTree tracks()    const { return root.getChildWithName (IDs::tracks); }
    juce::ValueTree patterns()  const { return root.getChildWithName (IDs::patterns); }
    juce::ValueTree mixer()     const { return root.getChildWithName (IDs::mixer); }

    // Todas las ediciones de usuario pasan por aquí para ser deshacibles.
    void setProperty (const juce::ValueTree& tree,
                      const juce::Identifier& prop,
                      const juce::var& newValue,
                      const juce::String& actionName,
                      int coalesceTimeMs = 0);

    void addChild (const juce::ValueTree& parent,
                   const juce::ValueTree& childToAdd,
                   int index,
                   const juce::String& actionName);

    void removeChild (const juce::ValueTree& parent,
                      const juce::ValueTree& childToRemove,
                      const juce::String& actionName);

    void performUndo();
    void performRedo();

    bool save (const juce::File& file);
    bool load (const juce::File& file);

    bool isModified() const { return modified; }
    void markSaved()        { setModified (false); }
    juce::String projectName() const;

    juce::File currentFile;
    std::function<void()> onModifiedChanged;
    std::function<void()> onProjectLoaded;

private:
    void applyFrom (const juce::ValueTree& newRoot);
    void setModified (bool shouldBeModified);

    bool modified = false;
};
