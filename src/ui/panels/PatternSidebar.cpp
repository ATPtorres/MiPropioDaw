#include "PatternSidebar.h"
#include "../Theme.h"
#include "../../core/IDs.h"

// ---------------------------------------------------------------------------
// PatternRow
// ---------------------------------------------------------------------------
PatternSidebar::PatternRow::PatternRow (juce::ValueTree patternTree, ProjectState& stateToUse,
                                         PatternSidebar& ownerToUse)
    : tree (std::move (patternTree)), state (stateToUse), owner (ownerToUse)
{
    nameLabel.setFont (Theme::font (12.5f));
    nameLabel.setEditable (true, true, true);
    nameLabel.onTextChange = [this]
    {
        state.setProperty (tree, IDs::name, nameLabel.getText().trim(), "Rename pattern");
    };
    addAndMakeVisible (nameLabel);

    deleteButton.setButtonText ("x");
    deleteButton.setColour (juce::TextButton::textColourOffId, Theme::textDim());
    deleteButton.onClick = [this]
    {
        auto parent = tree.getParent();
        if (parent.isValid())
            state.removeChild (parent, tree, "Delete pattern");
    };
    addAndMakeVisible (deleteButton);

    tree.addListener (this);
}

PatternSidebar::PatternRow::~PatternRow()
{
    tree.removeListener (this);
}

void PatternSidebar::PatternRow::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::accent2());
    g.fillRoundedRectangle (7.0f, (float) (getHeight() / 2 - 5), 10.0f, 10.0f, 2.0f);

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void PatternSidebar::PatternRow::resized()
{
    auto r = getLocalBounds().reduced (4, 2);
    deleteButton.setBounds (r.removeFromRight (18));
    r.removeFromLeft (16);
    nameLabel.setBounds (r);
}

void PatternSidebar::PatternRow::refresh()
{
    if (! nameLabel.hasKeyboardFocus (true))
        nameLabel.setText (tree.getProperty (IDs::name), juce::dontSendNotification);

    repaint();
}

void PatternSidebar::PatternRow::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refresh();
}

// ---------------------------------------------------------------------------
// PatternSidebar
// ---------------------------------------------------------------------------
PatternSidebar::PatternSidebar (ProjectState& stateToUse)
    : state (stateToUse), patterns (state.patterns())
{
    addButton.onClick = [this] { addPattern(); };
    addAndMakeVisible (addButton);

    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false, false, false);
    viewport.setScrollOnDragEnabled (false);
    addAndMakeVisible (viewport);

    patterns.addListener (this);
    rebuildRows();
}

PatternSidebar::~PatternSidebar()
{
    patterns.removeListener (this);
    rows.clear();
    viewport.setViewedComponent (nullptr, false);
}

void PatternSidebar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::text());
    g.setFont (Theme::font (11.0f, true));
    g.drawText ("PATTERNS", 8, 4, getWidth() - 40, 16, juce::Justification::centredLeft);

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void PatternSidebar::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);

    auto header = r.removeFromTop (26).reduced (6, 2);
    addButton.setBounds (header.removeFromRight (26));

    viewport.setBounds (r.reduced (2, 2));
    layoutRows();
}

void PatternSidebar::addPattern()
{
    juce::ValueTree tree (IDs::pattern);

    juce::String name;
    for (int n = 1; n < 10000; ++n)
    {
        name = "Pattern " + juce::String (n);
        bool found = false;
        for (int i = 0; i < patterns.getNumChildren(); ++i)
            if (patterns.getChild (i).getProperty (IDs::name).toString() == name)
            { found = true; break; }

        if (! found)
            break;
    }

    tree.setProperty (IDs::name, name, nullptr);
    state.addChild (patterns, tree, -1, "Add pattern");
}

void PatternSidebar::rebuildRows()
{
    rows.clear();

    for (int i = 0; i < patterns.getNumChildren(); ++i)
    {
        auto row = std::make_unique<PatternRow> (patterns.getChild (i), state, *this);
        rowHolder.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }

    layoutRows();
}

void PatternSidebar::layoutRows()
{
    const auto w = juce::jmax (0, viewport.getWidth() - 8);

    for (size_t i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (4, (int) i * kRowHeight, w, kRowHeight - 1);

    rowHolder.setSize (viewport.getWidth(), juce::jmax (viewport.getHeight(),
                                                        (int) rows.size() * kRowHeight + 4));
}

void PatternSidebar::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == patterns)
        rebuildRows();
}

void PatternSidebar::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == patterns)
        rebuildRows();
}

void PatternSidebar::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    // las filas escuchan sus propios árboles
}
