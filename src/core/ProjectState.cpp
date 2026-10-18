#include "ProjectState.h"

namespace
{
    // ------------------------------------------------------------------
    // Acciones genéricas de deshacer
    // ------------------------------------------------------------------
    class SetPropAction : public juce::UndoableAction
    {
    public:
        SetPropAction (juce::ValueTree t, juce::Identifier p, juce::var newValue)
            : tree (std::move (t)), prop (std::move (p)),
              old_value (tree.getProperty (prop)), new_value (std::move (newValue)) {}

        bool perform() override   { tree.setProperty (prop, new_value, nullptr); return true; }
        bool undo() override      { tree.setProperty (prop, old_value, nullptr); return true; }
        int getSizeInUnits() override { return 32; }

    private:
        juce::ValueTree tree;
        juce::Identifier prop;
        juce::var old_value, new_value;
    };

    class AddChildAction : public juce::UndoableAction
    {
    public:
        AddChildAction (juce::ValueTree parentTree, juce::ValueTree childTemplate, int childIndex)
            : parent (std::move (parentTree)), child_template (std::move (childTemplate)),
              index (childIndex) {}

        bool perform() override
        {
            auto insertionIndex = index;
            if (insertionIndex < 0 || insertionIndex > parent.getNumChildren())
                insertionIndex = parent.getNumChildren();

            added = child_template.createCopy();
            parent.addChild (added, insertionIndex, nullptr);
            return true;
        }

        bool undo() override
        {
            if (added.isValid())
                parent.removeChild (added, nullptr);
            return true;
        }

        int getSizeInUnits() override { return 128; }

    private:
        juce::ValueTree parent, child_template, added;
        int index;
    };

    class RemoveChildAction : public juce::UndoableAction
    {
    public:
        RemoveChildAction (juce::ValueTree parentTree, juce::ValueTree childToRemove)
            : parent (std::move (parentTree)), removed (std::move (childToRemove)),
              index (parent.indexOf (removed)) {}

        bool perform() override
        {
            if (removed.isValid() && removed.getParent() == parent)
                parent.removeChild (removed, nullptr);
            return true;
        }

        bool undo() override
        {
            auto idx = juce::jlimit (0, parent.getNumChildren(), index);
            parent.addChild (removed, idx, nullptr);
            return true;
        }

        int getSizeInUnits() override { return 128; }

    private:
        juce::ValueTree parent, removed;
        int index;
    };
}

// ------------------------------------------------------------------
ProjectState::ProjectState()
{
    root = juce::ValueTree (IDs::project);
    root.setProperty (IDs::version, 1, nullptr);

    auto tr = root.getOrCreateChildWithName (IDs::transport, nullptr);
    tr.setProperty (IDs::bpm,        140.0, nullptr);
    tr.setProperty (IDs::timeSigNum, 4,     nullptr);
    tr.setProperty (IDs::timeSigDen, 4,     nullptr);
    tr.setProperty (IDs::metronome,  false, nullptr);
    tr.setProperty (IDs::loop,       false, nullptr);
    tr.setProperty (IDs::record,     false, nullptr);

    root.getOrCreateChildWithName (IDs::tracks,   nullptr);
    root.getOrCreateChildWithName (IDs::patterns, nullptr);

    auto mx = root.getOrCreateChildWithName (IDs::mixer, nullptr);
    mx.setProperty (IDs::masterGain, 0.0f, nullptr);
}

void ProjectState::setProperty (const juce::ValueTree& tree,
                                const juce::Identifier& prop,
                                const juce::var& newValue,
                                const juce::String& actionName,
                                int coalesceTimeMs)
{
    if (! tree.isValid() || tree.getProperty (prop) == newValue)
        return;

    // coalesceTimeMs > 0 → la acción se une a la transacción actual
    // (útil para faders/parametrías arrastrables con beginNewTransaction en dragStart)
    if (coalesceTimeMs == 0)
        undo.beginNewTransaction();

    undo.perform (new SetPropAction (tree, prop, newValue), actionName);
    setModified (true);
}

void ProjectState::addChild (const juce::ValueTree& parent,
                             const juce::ValueTree& childToAdd,
                             int index,
                             const juce::String& actionName)
{
    if (! parent.isValid() || ! childToAdd.isValid())
        return;

    undo.beginNewTransaction();
    undo.perform (new AddChildAction (parent, childToAdd, index), actionName);
    setModified (true);
}

void ProjectState::removeChild (const juce::ValueTree& parent,
                                const juce::ValueTree& childToRemove,
                                const juce::String& actionName)
{
    if (! parent.isValid() || ! childToRemove.isValid())
        return;

    undo.beginNewTransaction();
    undo.perform (new RemoveChildAction (parent, childToRemove), actionName);
    setModified (true);
}

void ProjectState::performUndo()
{
    if (undo.canUndo())
    {
        undo.undo();
        setModified (true);
    }
}

void ProjectState::performRedo()
{
    if (undo.canRedo())
    {
        undo.redo();
        setModified (true);
    }
}

bool ProjectState::save (const juce::File& file)
{
    if (auto xml = root.createXml())
    {
        if (xml->writeTo (file))
        {
            currentFile = file;
            setModified (false);
            return true;
        }
    }
    return false;
}

bool ProjectState::load (const juce::File& file)
{
    auto xml = juce::parseXML (file);
    if (xml == nullptr)
        return false;

    auto newRoot = juce::ValueTree::fromXml (*xml);
    if (! newRoot.isValid())
        return false;

    applyFrom (newRoot);
    undo.clearUndoHistory();
    currentFile = file;
    setModified (false);

    if (onProjectLoaded)
        onProjectLoaded();

    return true;
}

// Mantiene la identidad de los ValueTree conocidos (transport/tracks/…)
// para que los listeners de la UI y del motor de audio no se rompan.
void ProjectState::applyFrom (const juce::ValueTree& newRoot)
{
    root.copyPropertiesFrom (newRoot, nullptr);

    const juce::Identifier names[] = { IDs::transport, IDs::tracks, IDs::patterns, IDs::mixer };

    for (auto id : names)
    {
        auto target = root.getOrCreateChildWithName (id, nullptr);
        auto source = newRoot.getChildWithName (id);

        target.copyPropertiesFrom (source, nullptr);

        while (target.getNumChildren() > 0)
            target.removeChild (0, nullptr);

        for (int i = 0; i < source.getNumChildren(); ++i)
            target.addChild (source.getChild (i).createCopy(), -1, nullptr);
    }
}

juce::String ProjectState::projectName() const
{
    if (currentFile != juce::File())
        return currentFile.getFileNameWithoutExtension();

    return "Untitled";
}

void ProjectState::setModified (bool shouldBeModified)
{
    if (modified == shouldBeModified)
        return;

    modified = shouldBeModified;
    if (onModifiedChanged)
        onModifiedChanged();
}
