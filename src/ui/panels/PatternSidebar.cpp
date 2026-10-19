#include "PatternSidebar.h"
#include "../Theme.h"
#include "../../core/IDs.h"

// ---------------------------------------------------------------------------
// PatternRow
// ---------------------------------------------------------------------------
PatternSidebar::PatternRow::PatternRow (juce::ValueTree patternTree, ProjectState& stateToUse,
                                        PatternLibrary& libraryToUse)
    : tree (std::move (patternTree)), state (stateToUse), library (libraryToUse)
{
    nameLabel.setFont (Theme::font (12.5f));
    nameLabel.setEditable (true, true, true);
    nameLabel.onTextChange = [this]
    {
        state.setProperty (tree, IDs::name, nameLabel.getText().trim(), "Rename pattern");
    };
    addAndMakeVisible (nameLabel);

    dupButton.setButtonText ("D");
    dupButton.setTooltip ("Duplicate pattern");
    dupButton.setColour (juce::TextButton::textColourOffId, Theme::accent2());
    dupButton.onClick = [this] { library.duplicatePattern (tree); };
    addAndMakeVisible (dupButton);

    deleteButton.setButtonText ("x");
    deleteButton.setColour (juce::TextButton::textColourOffId, Theme::textDim());
    deleteButton.onClick = [this]
    {
        if (! library.deletePattern (tree))
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle ("Pattern in use")
                    .withMessage ("El patrón está referenciado por clips del arreglo.\n"
                                  "Borra o mueve esos clips antes de eliminarlo.")
                    .withButton ("OK")
                    .withAssociatedComponent (this),
                [] (int) {});
        }
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
    g.fillAll (selected ? Theme::panelHi() : Theme::panel());

    if (selected)
    {
        g.setColour (Theme::accent());
        g.fillRect (0, 0, 3, getHeight());
    }

    g.setColour (Theme::accent2());
    g.fillRoundedRectangle (7.0f, (float) (getHeight() / 2 - 5), 10.0f, 10.0f, 2.0f);

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void PatternSidebar::PatternRow::resized()
{
    auto r = getLocalBounds().reduced (4, 2);
    deleteButton.setBounds (r.removeFromRight (20));
    r.removeFromRight (2);
    dupButton.setBounds (r.removeFromRight (20));
    r.removeFromLeft (14);
    nameLabel.setBounds (r);
}

void PatternSidebar::PatternRow::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == this || e.eventComponent == &nameLabel)
        library.setActivePatternUid (getPatternUid());
}

void PatternSidebar::PatternRow::setSelected (bool s)
{
    if (selected == s)
        return;

    selected = s;
    repaint();
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
// ClipRow
// ---------------------------------------------------------------------------
PatternSidebar::ClipRow::ClipRow (juce::ValueTree trackTree, juce::ValueTree clipTree,
                                  ProjectState& stateToUse, AudioEngine& engineToUse,
                                  PatternLibrary& libraryToUse)
    : track (std::move (trackTree)), clip (std::move (clipTree)),
      state (stateToUse), engine (engineToUse), library (libraryToUse)
{
    label.setFont (Theme::font (11.5f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    uniqueButton.setButtonText ("U");
    uniqueButton.setTooltip ("Make unique — duplica el patrón para este clip");
    uniqueButton.onClick = [this]
    {
        library.makeUnique (clip);
        refresh();
    };
    addAndMakeVisible (uniqueButton);

    deleteButton.setButtonText ("x");
    deleteButton.onClick = [this]
    {
        library.deleteClip (track, clip);
    };
    addAndMakeVisible (deleteButton);

    refresh();
}

PatternSidebar::ClipRow::~ClipRow() = default;

void PatternSidebar::ClipRow::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    const auto colour = juce::Colour ((juce::uint32) (juce::int64) track.getProperty (IDs::colour, (juce::int64) 0xff4fc3f7));
    g.setColour (colour);
    g.fillRoundedRectangle (7.0f, (float) (getHeight() / 2 - 5), 10.0f, 10.0f, 2.0f);

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void PatternSidebar::ClipRow::resized()
{
    auto r = getLocalBounds().reduced (4, 2);
    deleteButton.setBounds (r.removeFromRight (20));
    r.removeFromRight (2);
    uniqueButton.setBounds (r.removeFromRight (20));
    r.removeFromLeft (14);
    label.setBounds (r);
}

void PatternSidebar::ClipRow::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &uniqueButton || e.eventComponent == &deleteButton)
        return;

    library.setActivePatternUid ((int) clip.getProperty (IDs::patternUid, -1));
    engine.setPlayheadBeats ((double) clip.getProperty (IDs::start, 0.0));
}

void PatternSidebar::ClipRow::refresh()
{
    auto pat = library.patternForUid ((int) clip.getProperty (IDs::patternUid, -1));
    const auto patName = pat.getProperty (IDs::name, "?").toString();
    const auto trackName = track.getProperty (IDs::name, "?").toString();
    const auto start = (double) clip.getProperty (IDs::start, 0.0);

    label.setText (trackName + " · " + patName + " @ " + juce::String (start, 1),
                   juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// PatternSidebar
// ---------------------------------------------------------------------------
PatternSidebar::PatternSidebar (ProjectState& stateToUse, AudioEngine& engineToUse)
    : state (stateToUse), engine (engineToUse), library (stateToUse),
      patternsTree (stateToUse.patterns())
{
    addButton.onClick = [this] { addPattern(); };
    addAndMakeVisible (addButton);

    patternViewport.setViewedComponent (&patternHolder, false);
    patternViewport.setScrollBarsShown (true, false, false, false);
    addAndMakeVisible (patternViewport);

    clipViewport.setViewedComponent (&clipHolder, false);
    clipViewport.setScrollBarsShown (true, false, false, false);
    addAndMakeVisible (clipViewport);

    state.root.addListener (this);
    rebuildRows();
}

PatternSidebar::~PatternSidebar()
{
    state.root.removeListener (this);
    patternRows.clear();
    clipRows.clear();
    patternViewport.setViewedComponent (nullptr, false);
    clipViewport.setViewedComponent (nullptr, false);
}

void PatternSidebar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    auto r = getLocalBounds();

    g.setColour (Theme::text());
    g.setFont (Theme::font (11.0f, true));
    g.drawText ("PATTERNS", 8, 4, getWidth() - 40, 16, juce::Justification::centredLeft);

    const auto clipTop = patternViewport.getBottom() + 2;

    g.drawText ("CLIPS", 8, clipTop, getWidth() - 16, kHeaderH, juce::Justification::centredLeft);

    g.setColour (Theme::border());
    g.fillRect (0, clipTop - 1, getWidth(), 1);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    juce::ignoreUnused (r);
}

void PatternSidebar::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kHeaderH);

    auto header = r.removeFromTop (26).reduced (6, 2);
    addButton.setBounds (header.removeFromRight (26));

    auto clipArea = r.removeFromBottom (juce::jmax (60, r.getHeight() / 2));
    clipArea.removeFromTop (kHeaderH);
    clipViewport.setBounds (clipArea.reduced (2, 2));

    patternViewport.setBounds (r.reduced (2, 2));

    layoutRows();
}

void PatternSidebar::addPattern()
{
    library.addPattern();
}

void PatternSidebar::rebuildRows()
{
    patternRows.clear();
    clipRows.clear();

    for (int i = 0; i < patternsTree.getNumChildren(); ++i)
    {
        auto row = std::make_unique<PatternRow> (patternsTree.getChild (i), state, library);
        patternHolder.addAndMakeVisible (*row);
        patternRows.push_back (std::move (row));
    }

    auto tracks = library.tracks();
    for (int t = 0; t < tracks.getNumChildren(); ++t)
    {
        auto track = tracks.getChild (t);
        for (int c = 0; c < track.getNumChildren(); ++c)
        {
            auto clip = track.getChild (c);
            if (clip.getType() != juce::Identifier (IDs::clip))
                continue;

            auto row = std::make_unique<ClipRow> (track, clip, state, engine, library);
            clipHolder.addAndMakeVisible (*row);
            clipRows.push_back (std::move (row));
        }
    }

    updateSelection();
    layoutRows();
}

void PatternSidebar::layoutRows()
{
    const auto pw = juce::jmax (0, patternViewport.getWidth() - 8);
    for (size_t i = 0; i < patternRows.size(); ++i)
        patternRows[i]->setBounds (4, (int) i * kRowHeight, pw, kRowHeight - 1);

    patternHolder.setSize (patternViewport.getWidth(),
                           juce::jmax (patternViewport.getHeight(), (int) patternRows.size() * kRowHeight + 4));

    const auto cw = juce::jmax (0, clipViewport.getWidth() - 8);
    for (size_t i = 0; i < clipRows.size(); ++i)
        clipRows[i]->setBounds (4, (int) i * kRowHeight, cw, kRowHeight - 1);

    clipHolder.setSize (clipViewport.getWidth(),
                        juce::jmax (clipViewport.getHeight(), (int) clipRows.size() * kRowHeight + 4));
}

void PatternSidebar::updateSelection()
{
    const int active = (int) state.root.getProperty (IDs::activePattern, -1);

    for (auto& row : patternRows)
        row->setSelected ((int) row->getPatternUid() == active);
}

void PatternSidebar::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == patternsTree || parent.getType() == juce::Identifier (IDs::track))
        rebuildRows();
}

void PatternSidebar::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == patternsTree || parent.getType() == juce::Identifier (IDs::track))
        rebuildRows();
}

void PatternSidebar::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    if (tree == state.root && prop == juce::Identifier (IDs::activePattern))
        updateSelection();
    else if (tree.getType() == juce::Identifier (IDs::clip) && prop == juce::Identifier (IDs::patternUid))
        rebuildRows();
}
