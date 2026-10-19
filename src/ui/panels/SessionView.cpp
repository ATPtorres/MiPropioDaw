#include "SessionView.h"
#include "../Theme.h"
#include "../../core/IDs.h"

namespace
{
    constexpr int kQuantOff  = 1;
    constexpr int kQuantBeat = 2;
    constexpr int kQuant2    = 3;
    constexpr int kQuant4    = 4;
    constexpr int kQuant8    = 5;
    constexpr int kQuantBar  = 6;
}

SessionView::ClipCell::ClipCell (SessionView& ownerToUse, juce::ValueTree trackTree,
                                 int sceneUidToUse, bool isMidiTrack)
    : owner (ownerToUse), track (std::move (trackTree)), sceneUid (sceneUidToUse), midi (isMidiTrack)
{
}

juce::ValueTree SessionView::ClipCell::clipTree() const
{
    return owner.library.sessionClipAt (track, sceneUid);
}

int SessionView::ClipCell::slotIndex() const
{
    return (int) track.getProperty (IDs::mixerSlot, -1);
}

void SessionView::ClipCell::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto colour = juce::Colour ((juce::uint32) (juce::int64) track.getProperty (IDs::colour,
                                       (juce::int64) 0xff4fc3f7));

    auto clip = clipTree();

    if (! midi)
    {
        g.setColour (Theme::panelAlt());
        g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
        g.setColour (Theme::border());
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Theme::textDim().withAlpha (0.4f));
        g.setFont (Theme::font (10.0f));
        g.drawFittedText ("audio", getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    if (! clip.isValid())
    {
        g.setColour (Theme::panel());
        g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
        g.setColour (Theme::border());
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Theme::textDim().withAlpha (0.45f));
        g.setFont (Theme::font (14.0f));
        g.drawText ("+", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const int slot  = slotIndex();
    const int puid  = (int) clip.getProperty (IDs::patternUid, -1);
    const bool playing = owner.engine.isSessionActive (slot) && owner.engine.sessionPatternFor (slot) == puid;
    const bool queued  = ! playing && owner.engine.sessionQueuedPatternFor (slot) == puid;

    g.setColour (colour.withAlpha (playing ? 0.55f : 0.22f));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);

    g.setColour (playing ? Theme::success()
                         : (queued ? Theme::accent2() : colour.withAlpha (0.6f)));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, playing ? 2.0f : 1.0f);

    auto pat = owner.library.patternForUid (puid);
    const auto patName = pat.isValid() ? pat.getProperty (IDs::name, "?").toString()
                                       : juce::String ("?");

    g.setColour (Theme::text());
    g.setFont (Theme::font (11.5f, playing));
    g.drawFittedText (patName, getLocalBounds().reduced (6, 2), juce::Justification::centred, 2);

    if (queued)
    {
        g.setColour (Theme::accent2());
        g.setFont (Theme::font (9.0f));
        g.drawText ("QUEUED", 4, 4, getWidth() - 8, 12, juce::Justification::centredLeft);
    }

    if (playing)
    {
        const float prog = juce::jlimit (0.0f, 1.0f, owner.engine.sessionLoopProgress (slot));
        g.setColour (Theme::accent());
        g.fillRect (3.0f, (float) getHeight() - 5.0f,
                    (float) (getWidth() - 6) * prog, 2.0f);
    }
}

void SessionView::ClipCell::mouseDown (const juce::MouseEvent& e)
{
    if (! midi)
        return;

    auto clip = clipTree();

    if (e.mods.isRightButtonDown())
    {
        if (clip.isValid())
            owner.library.deleteSessionClip (track, clip);
        return;
    }

    if (! e.mods.isLeftButtonDown())
        return;

    const int slot = slotIndex();
    if (slot < 0)
        return;

    if (clip.isValid())
    {
        const int puid = (int) clip.getProperty (IDs::patternUid, -1);

        owner.library.setActivePatternUid (puid);

        if (owner.engine.isSessionActive (slot) && owner.engine.sessionPatternFor (slot) == puid)
            owner.engine.stopSessionClip (slot);
        else
            owner.engine.launchSessionClip (slot, puid);
    }
    else
    {
        const int active = owner.library.activePatternUid();
        if (active >= 0)
            owner.library.addSessionClip (track, sceneUid, active);
    }
}

// ---------------------------------------------------------------------------

SessionView::SceneRow::SceneRow (SessionView& ownerToUse, juce::ValueTree sceneTree)
    : owner (ownerToUse), scene (std::move (sceneTree))
{
    launchButton.setTooltip ("Lanza todos los clips de esta escena");
    launchButton.setColour (juce::TextButton::textColourOffId, Theme::success());
    launchButton.onClick = [this] { owner.launchScene (scene); };
    addAndMakeVisible (launchButton);

    nameLabel.setFont (Theme::font (12.0f));
    nameLabel.setEditable (true, true, true);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.onTextChange = [this]
    {
        owner.library.renameScene (scene, nameLabel.getText().trim());
    };
    addAndMakeVisible (nameLabel);

    deleteButton.setColour (juce::TextButton::textColourOffId, Theme::textDim());
    deleteButton.onClick = [this] { owner.library.deleteScene (scene); };
    addAndMakeVisible (deleteButton);

    scene.addListener (this);
    refreshName();
}

SessionView::SceneRow::~SceneRow()
{
    scene.removeListener (this);
}

void SessionView::SceneRow::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panelAlt());

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void SessionView::SceneRow::resized()
{
    auto r = getLocalBounds().reduced (4, 4);

    launchButton.setBounds (r.removeFromLeft (26));
    r.removeFromLeft (4);
    deleteButton.setBounds (r.removeFromRight (20));
    nameLabel.setBounds (r);
}

void SessionView::SceneRow::refreshName()
{
    if (! nameLabel.hasKeyboardFocus (true))
        nameLabel.setText (scene.getProperty (IDs::name, "?").toString(),
                           juce::dontSendNotification);
}

void SessionView::SceneRow::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    juce::ignoreUnused (tree, prop);
    refreshName();
}

// ---------------------------------------------------------------------------

SessionView::SessionView (ProjectState& stateToUse, AudioEngine& engineToUse, TrackList& trackListToUse)
    : state (stateToUse), engine (engineToUse), trackList (trackListToUse), library (stateToUse),
      tracksTree (stateToUse.tracks()), sessionTree (stateToUse.session())
{
    juce::ignoreUnused (trackList);

    titleLabel.setFont (Theme::font (13.0f, true));
    titleLabel.setColour (juce::Label::textColourId, Theme::accent());
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    launchLabel.setFont (Theme::font (10.0f));
    launchLabel.setColour (juce::Label::textColourId, Theme::textDim());
    launchLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (launchLabel);

    quantizeBox.addItem ("Off", kQuantOff);
    quantizeBox.addItem ("Beat", kQuantBeat);
    quantizeBox.addItem ("2 beats", kQuant2);
    quantizeBox.addItem ("4 beats", kQuant4);
    quantizeBox.addItem ("8 beats", kQuant8);
    quantizeBox.addItem ("Bar", kQuantBar);
    quantizeBox.onChange = [this]
    {
        if (ignoreQuantizeCallback)
            return;

        const float beats = selectedQuantize();
        library.setQuantizeBeats (beats);
        engine.setSessionQuantize (beats);
    };
    addAndMakeVisible (quantizeBox);

    stopAllButton.onClick = [this] { engine.stopAllSessionClips(); };
    addAndMakeVisible (stopAllButton);

    hintLabel.setFont (Theme::font (10.0f));
    hintLabel.setColour (juce::Label::textColourId, Theme::textDim());
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    hintLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (hintLabel);

    addSceneButton = std::make_unique<juce::TextButton>("+ Scene", juce::String());
    addSceneButton->onClick = [this] { library.addScene(); };
    content.addAndMakeVisible (*addSceneButton);

    viewport.setViewedComponent (&content, false);
    addAndMakeVisible (viewport);

    state.root.addListener (this);

    engine.setSessionQuantize (library.quantizeBeats());
    refreshQuantize();
    rebuildGrid();

    startTimerHz (20);
}

SessionView::~SessionView()
{
    stopTimer();
    state.root.removeListener (this);
    viewport.setViewedComponent (nullptr, false);
    cells.clear();
    sceneRows.clear();
    trackLabels.clear();
    trackStopButtons.clear();
    addSceneButton.reset();
}

void SessionView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::border());
    g.fillRect (0, kToolbarH, getWidth(), 1);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void SessionView::resized()
{
    auto r = getLocalBounds();

    auto toolbar = r.removeFromTop (kToolbarH).reduced (8, 4);

    titleLabel.setBounds (toolbar.removeFromLeft (76));
    toolbar.removeFromLeft (8);
    launchLabel.setBounds (toolbar.removeFromLeft (54));
    toolbar.removeFromLeft (4);
    quantizeBox.setBounds (toolbar.removeFromLeft (104));
    stopAllButton.setBounds (toolbar.removeFromRight (86));
    toolbar.removeFromRight (8);
    hintLabel.setBounds (toolbar);

    viewport.setBounds (r);

    layoutGrid();
}

float SessionView::selectedQuantize() const
{
    const int id = quantizeBox.getSelectedId();

    switch (id)
    {
        case kQuantOff:  return 0.0f;
        case kQuantBeat: return 1.0f;
        case kQuant2:    return 2.0f;
        case kQuant4:    return 4.0f;
        case kQuant8:    return 8.0f;
        case kQuantBar:  return (float) juce::jmax (1, (int) state.transport().getProperty (IDs::timeSigNum, 4));
        default:         return library.quantizeBeats();
    }
}

void SessionView::refreshQuantize()
{
    ignoreQuantizeCallback = true;

    const float q = library.quantizeBeats();
    int id = 0;

    if (q <= 0.0f)          id = kQuantOff;
    else if (q == 1.0f)     id = kQuantBeat;
    else if (q == 2.0f)     id = kQuant2;
    else if (q == 4.0f)     id = kQuant4;
    else if (q == 8.0f)     id = kQuant8;
    else if (q == (float) juce::jmax (1, (int) state.transport().getProperty (IDs::timeSigNum, 4)))
                            id = kQuantBar;

    quantizeBox.setSelectedId (id, juce::dontSendNotification);
    ignoreQuantizeCallback = false;
}

void SessionView::launchScene (const juce::ValueTree& scene)
{
    const int sceneUid = (int) scene.getProperty (IDs::uid, -1);
    auto tracks = library.tracks();

    for (int t = 0; t < tracks.getNumChildren(); ++t)
    {
        auto track = tracks.getChild (t);
        if (track.getProperty (IDs::type).toString() != "midi")
            continue;

        const int slot = (int) track.getProperty (IDs::mixerSlot, -1);
        if (slot < 0)
            continue;

        auto clip = library.sessionClipAt (track, sceneUid);

        if (clip.isValid())
            engine.launchSessionClip (slot, (int) clip.getProperty (IDs::patternUid, -1));
        else
            engine.stopSessionClip (slot);
    }
}

void SessionView::timerCallback()
{
    if (! engine.isPlaying())
        return;

    for (auto& cell : cells)
        cell->repaint();
}

void SessionView::rebuildGrid()
{
    cells.clear();
    sceneRows.clear();
    trackLabels.clear();
    trackStopButtons.clear();

    auto tracks = library.tracks();
    auto session = library.session();

    for (int t = 0; t < tracks.getNumChildren(); ++t)
    {
        auto track = tracks.getChild (t);

        auto label = std::make_unique<juce::Label>();
        label->setFont (Theme::font (11.5f, true));
        label->setJustificationType (juce::Justification::centredLeft);
        label->setInterceptsMouseClicks (false, false);
        label->setText (track.getProperty (IDs::name, "?").toString(), juce::dontSendNotification);
        label->setColour (juce::Label::textColourId,
                          juce::Colour ((juce::uint32) (juce::int64) track.getProperty (IDs::colour,
                                         (juce::int64) 0xff4fc3f7)));
        content.addAndMakeVisible (*label);
        trackLabels.push_back (std::move (label));

        if (track.getProperty (IDs::type).toString() == "midi")
        {
            const int slot = (int) track.getProperty (IDs::mixerSlot, -1);

            auto stopBtn = std::make_unique<juce::TextButton> ("STOP", juce::String());
            stopBtn->setColour (juce::TextButton::textColourOffId, Theme::textDim());
            stopBtn->setTooltip ("Detiene el clip de sesión de esta pista");
            stopBtn->onClick = [this, slot] { engine.stopSessionClip (slot); };
            content.addAndMakeVisible (*stopBtn);
            trackStopButtons.push_back (std::move (stopBtn));
        }
        else
        {
            trackStopButtons.push_back (nullptr);
        }
    }

    if (session.isValid())
    {
        for (int s = 0; s < session.getNumChildren(); ++s)
        {
            auto scene = session.getChild (s);
            sceneRows.push_back (std::make_unique<SceneRow> (*this, scene));

            const int sceneUid = (int) scene.getProperty (IDs::uid, -1);

            for (int t = 0; t < tracks.getNumChildren(); ++t)
            {
                auto track = tracks.getChild (t);
                const bool isMidi = track.getProperty (IDs::type).toString() == "midi";
                cells.push_back (std::make_unique<ClipCell> (*this, track, sceneUid, isMidi));
            }
        }
    }

    for (auto& row : sceneRows)
        content.addAndMakeVisible (*row);

    for (auto& cell : cells)
        content.addAndMakeVisible (*cell);

    layoutGrid();
}

void SessionView::layoutGrid()
{
    const int numRows = (int) sceneRows.size();
    const int numCols = (int) trackLabels.size();

    for (int t = 0; t < numCols; ++t)
    {
        auto& label = *trackLabels[(size_t) t];

        auto col = juce::Rectangle<int> (kSceneColW + t * kCellW, 0, kCellW, kHeaderH);
        auto top = col.removeFromTop (22);

        label.setBounds (top.reduced (6, 2));

        if (trackStopButtons[(size_t) t] != nullptr)
            trackStopButtons[(size_t) t]->setBounds (col.reduced (6, 2));
    }

    for (int s = 0; s < numRows; ++s)
    {
        sceneRows[(size_t) s]->setBounds (0, kHeaderH + s * kCellH, kSceneColW - 2, kCellH - 2);

        for (int t = 0; t < numCols; ++t)
        {
            auto* cell = cells[(size_t) (s * numCols + t)].get();
            cell->setBounds (kSceneColW + t * kCellW, kHeaderH + s * kCellH,
                             kCellW - 2, kCellH - 2);
        }
    }

    const int contentW = juce::jmax (viewport.getWidth(),
                                     kSceneColW + numCols * kCellW + 8);
    const int contentH = juce::jmax (viewport.getHeight(),
                                     kHeaderH + numRows * kCellH + 38);

    content.setSize (contentW, contentH);

    addSceneButton->setBounds (6, kHeaderH + numRows * kCellH + 6, kSceneColW - 14, 26);
}

void SessionView::refreshTrackLabels()
{
    auto tracks = library.tracks();

    for (int t = 0; t < (int) trackLabels.size() && t < tracks.getNumChildren(); ++t)
    {
        auto track = tracks.getChild (t);
        trackLabels[(size_t) t]->setText (track.getProperty (IDs::name, "?").toString(),
                                          juce::dontSendNotification);
        trackLabels[(size_t) t]->setColour (juce::Label::textColourId,
            juce::Colour ((juce::uint32) (juce::int64) track.getProperty (IDs::colour,
                           (juce::int64) 0xff4fc3f7)));
    }
}

void SessionView::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == tracksTree || parent == sessionTree
        || parent.getType() == juce::Identifier (IDs::track))
        scheduleRebuild();
}

void SessionView::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == tracksTree || parent == sessionTree
        || parent.getType() == juce::Identifier (IDs::track))
        scheduleRebuild();
}

void SessionView::scheduleRebuild()
{
    if (rebuildPending)
        return;

    rebuildPending = true;

    juce::Component::SafePointer<SessionView> safe (this);

    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr)
        {
            safe->rebuildPending = false;
            safe->rebuildGrid();
        }
    });
}

void SessionView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    if (tree.getType() == juce::Identifier (IDs::track)
        && (prop == juce::Identifier (IDs::name) || prop == juce::Identifier (IDs::colour)))
    {
        refreshTrackLabels();
        content.repaint();
    }
    else if (tree.getType() == juce::Identifier (IDs::pattern)
             && prop == juce::Identifier (IDs::name))
    {
        content.repaint();
    }
    else if (tree.getType() == juce::Identifier (IDs::sessionClip)
             && prop == juce::Identifier (IDs::patternUid))
    {
        content.repaint();
    }
    else if (tree == sessionTree && prop == juce::Identifier (IDs::quantize))
    {
        refreshQuantize();
        engine.setSessionQuantize (library.quantizeBeats());
    }
}
