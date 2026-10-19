#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../audio/AudioEngine.h"
#include "../../model/PatternLibrary.h"

// Barra lateral FL Studio: lista de patrones (seleccionar, duplicar, borrar,
// renombrar) y lista de clips del arreglo (Make Unique, borrar, saltar).
class PatternSidebar : public juce::Component,
                       private juce::ValueTree::Listener
{
public:
    PatternSidebar (ProjectState& state, AudioEngine& engine);
    ~PatternSidebar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void addPattern();

private:
    class PatternRow : public juce::Component,
                       private juce::ValueTree::Listener
    {
    public:
        PatternRow (juce::ValueTree patternTree, ProjectState& state, PatternLibrary& lib);
        ~PatternRow() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;
        void setSelected (bool s);
        void refresh();
        int getPatternUid() const { return library.uidOf (tree); }

    private:
        void valueTreePropertyChanged (juce::ValueTree& tree,
                                       const juce::Identifier& prop) override;

        juce::ValueTree tree;
        ProjectState& state;
        PatternLibrary& library;
        juce::Label nameLabel;
        juce::TextButton dupButton { juce::String(), {} };
        juce::TextButton deleteButton { juce::String(), {} };
        bool selected = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternRow)
    };

    class ClipRow : public juce::Component
    {
    public:
        ClipRow (juce::ValueTree trackTree, juce::ValueTree clipTree,
                 ProjectState& state, AudioEngine& engine, PatternLibrary& lib);
        ~ClipRow() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;
        void refresh();

    private:
        juce::ValueTree track, clip;
        ProjectState& state;
        AudioEngine& engine;
        PatternLibrary& library;
        juce::Label label;
        juce::TextButton uniqueButton { "U", {} };
        juce::TextButton deleteButton { juce::String(), {} };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipRow)
    };

    void rebuildRows();
    void layoutRows();
    void updateSelection();

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    ProjectState& state;
    AudioEngine& engine;
    PatternLibrary library;
    juce::ValueTree patternsTree;

    juce::TextButton addButton { "+", {} };
    juce::Viewport patternViewport;
    juce::Viewport clipViewport;
    juce::Component patternHolder;
    juce::Component clipHolder;
    std::vector<std::unique_ptr<PatternRow>> patternRows;
    std::vector<std::unique_ptr<ClipRow>> clipRows;

    static constexpr int kRowHeight = 26;
    static constexpr int kHeaderH = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternSidebar)
};
