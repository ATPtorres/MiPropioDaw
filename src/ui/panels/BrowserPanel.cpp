#include "BrowserPanel.h"
#include "../Theme.h"

namespace
{
    const char* kCategories[] = {
        "Instruments",
        "Audio Effects",
        "MIDI Effects",
        "Clips & Samples",
        "User Folders"
    };
}

BrowserPanel::BrowserPanel()
{
    for (auto* c : kCategories)
        allItems.add (c);

    filtered = allItems;

    searchBox.setTextToShowWhenEmpty ("Search...", Theme::textDim());
    searchBox.setColour (juce::TextEditor::textColourId, Theme::text());
    searchBox.onTextChange = [this] { searchChanged(); };
    addAndMakeVisible (searchBox);

    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, Theme::panel());
    addAndMakeVisible (list);
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::text());
    g.setFont (Theme::font (11.0f, true));
    g.drawText ("BROWSER", 8, 4, getWidth() - 16, 16, juce::Justification::centredLeft);

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void BrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (0, 24);
    r.removeFromTop (4);
    searchBox.setBounds (r.removeFromTop (26).reduced (8, 0));
    r.removeFromTop (6);
    list.setBounds (r.reduced (4, 0));
}

int BrowserPanel::getNumRows()
{
    return filtered.size();
}

void BrowserPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
    {
        g.setColour (Theme::accent().withAlpha (0.16f));
        g.fillRect (0, 0, width, height);
    }

    g.setColour (selected ? Theme::accent() : Theme::text());
    g.setFont (Theme::font (12.5f));
    g.drawText ("  " + filtered[row], 6, 0, width - 12, height, juce::Justification::centredLeft);
}

void BrowserPanel::listBoxItemClicked (int, const juce::MouseEvent&) {}
void BrowserPanel::listBoxItemDoubleClicked (int, const juce::MouseEvent&) {}
void BrowserPanel::selectedRowsChanged (int) {}

void BrowserPanel::searchChanged()
{
    const auto query = searchBox.getText().trim();

    filtered.clear();
    for (auto& item : allItems)
        if (query.isEmpty() || item.containsIgnoreCase (query))
            filtered.add (item);

    list.updateContent();
    list.repaint();
}
