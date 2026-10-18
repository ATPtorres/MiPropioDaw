#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"

// Barra lateral de patrones (herencia FL Studio).
// Fase 1: estructura + alta/renombrado/borrado con undo.
// Fase 3: contenido MIDI, Make Unique y arrastre al arreglo.
class PatternSidebar : public juce::Component,
                       private juce::ValueTree::Listener
{
public:
    explicit PatternSidebar (ProjectState& state);
    ~PatternSidebar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void addPattern();

private:
    // Fila individual de patrón
    class PatternRow : public juce::Component,
                       private juce::ValueTree::Listener
    {
    public:
        PatternRow (juce::ValueTree patternTree, ProjectState& state, PatternSidebar& owner);
        ~PatternRow() override;

        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        void refresh();
        void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyChanged,
                                       const juce::Identifier& property) override;

        juce::ValueTree tree;
        ProjectState& state;
        PatternSidebar& owner;
        juce::Label nameLabel;
        juce::TextButton deleteButton { juce::String(), {} };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternRow)
    };

    void rebuildRows();
    void layoutRows();

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyChanged,
                                   const juce::Identifier& property) override;

    ProjectState& state;
    juce::ValueTree patterns;

    juce::TextButton addButton { "+", {} };
    juce::Viewport viewport;
    juce::Component rowHolder;   // contiene las filas (altura = n * 26)
    std::vector<std::unique_ptr<PatternRow>> rows;

    static constexpr int kRowHeight = 26;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternSidebar)
};
