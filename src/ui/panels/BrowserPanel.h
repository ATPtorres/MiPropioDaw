#pragma once

#include <JuceHeader.h>

// Panel izquierdo superior: navegador de archivos/plugins (estructura base;
// contenido real en Fase 4).
class BrowserPanel : public juce::Component,
                     private juce::ListBoxModel
{
public:
    BrowserPanel();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void searchChanged();

    juce::TextEditor searchBox;
    juce::ListBox list { "browser", this };
    juce::StringArray allItems;
    juce::StringArray filtered;
};
