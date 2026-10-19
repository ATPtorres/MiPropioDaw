#include "MidiEditorPanel.h"
#include "../Theme.h"
#include "../../core/IDs.h"

namespace
{
    const double snapDivs[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    const char*  snapNames[] { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
    constexpr int kNumSnaps = 6;

    const float zooms[] { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };
    constexpr int kNumZooms = 7;
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------
void MidiEditorPanel::Keyboard::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    for (int p = 127; p >= 0; --p)
    {
        const int y = owner.roll.yForPitch (p);
        const int pc = p % 12;
        const bool black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        const bool isC = (pc == 0);

        g.setColour (black ? juce::Colour (0xff141414) : juce::Colour (0xffc8c8c8));
        g.fillRect (0, y, getWidth(), PianoRoll::kRowHeight - 1);

        if (isC)
        {
            g.setColour (Theme::textDim());
            g.setFont (Theme::font (9.5f));
            g.drawText (Scales::noteName (p), 4, y + 1, getWidth() - 8, 12,
                        juce::Justification::centredLeft);
        }
    }

    g.setColour (Theme::border());
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void MidiEditorPanel::Keyboard::mouseDown (const juce::MouseEvent& e)
{
    lastPitch = juce::jlimit (0, 127, owner.roll.pitchForY (e.y));
    if (onKey != nullptr)
        onKey (lastPitch, true);
    repaint();
}

void MidiEditorPanel::Keyboard::mouseUp (const juce::MouseEvent&)
{
    if (lastPitch >= 0 && onKey != nullptr)
        onKey (lastPitch, false);
    lastPitch = -1;
}

// ---------------------------------------------------------------------------
// VelocityLane
// ---------------------------------------------------------------------------
int MidiEditorPanel::VelocityLane::noteAt (int x) const
{
    auto pattern = owner.library.activePattern();
    if (! pattern.isValid())
        return -1;

    const double pos = (double) x / owner.roll.pxPerBeat();

    for (int i = 0; i < pattern.getNumChildren(); ++i)
    {
        auto n = pattern.getChild (i);
        if (n.getType() != juce::Identifier (IDs::note))
            continue;

        const double s = (double) n.getProperty (IDs::start);
        const double d = (double) n.getProperty (IDs::duration);

        if (pos >= s && pos < s + d)
            return i;
    }

    return -1;
}

void MidiEditorPanel::VelocityLane::applyVelocity (int noteIndex, int y)
{
    auto pattern = owner.library.activePattern();
    if (! pattern.isValid() || ! juce::isPositiveAndBelow (noteIndex, pattern.getNumChildren()))
        return;

    const double vel = 1.0 - (double) y / (double) juce::jmax (1, getHeight());
    owner.state.setProperty (pattern.getChild (noteIndex), IDs::velocity,
                             juce::jlimit (0.05, 1.0, vel), "Velocity", 1);
}

void MidiEditorPanel::VelocityLane::mouseDown (const juce::MouseEvent& e)
{
    dragIndex = noteAt (e.x);
    if (dragIndex < 0)
        return;

    owner.state.undo.beginNewTransaction ("Velocity");
    applyVelocity (dragIndex, e.y);
}

void MidiEditorPanel::VelocityLane::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex >= 0)
        applyVelocity (dragIndex, e.y);
}

void MidiEditorPanel::VelocityLane::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::border());
    g.fillRect (0, 0, getWidth(), 1);

    auto pattern = owner.library.activePattern();
    if (! pattern.isValid())
        return;

    const auto colour = owner.roll.getTrackColour();

    for (int i = 0; i < pattern.getNumChildren(); ++i)
    {
        auto n = pattern.getChild (i);
        if (n.getType() != juce::Identifier (IDs::note))
            continue;

        const auto x = (float) ((double) n.getProperty (IDs::start) * owner.roll.pxPerBeat());
        const auto w = juce::jmax (3.0f, (float) ((double) n.getProperty (IDs::duration) * owner.roll.pxPerBeat()) - 1.0f);
        const auto vel = (float) (double) n.getProperty (IDs::velocity, 0.8);
        const auto barH = juce::jmax (2.0f, (float) getHeight() * vel - 2.0f);

        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (x, (float) getHeight() - barH, w, barH, 1.5f);
    }
}

// ---------------------------------------------------------------------------
// SyncedViewport
// ---------------------------------------------------------------------------
void MidiEditorPanel::SyncedViewport::visibleAreaChanged (const juce::Rectangle<int>& area)
{
    if (! owner.syncingScrollNow)
        owner.syncScroll (role, area);
}

// ---------------------------------------------------------------------------
// MidiEditorPanel
// ---------------------------------------------------------------------------
MidiEditorPanel::MidiEditorPanel (ProjectState& stateToUse, AudioEngine& engineToUse, TrackList& list)
    : state (stateToUse), engine (engineToUse), trackList (list), library (stateToUse),
      patternsTree (stateToUse.patterns()), roll (stateToUse, library)
{
    auto setupCombo = [this] (juce::ComboBox& b, const juce::String& tooltip)
    {
        b.setTooltip (tooltip);
        b.setTextWhenNothingSelected (tooltip);
        addAndMakeVisible (b);
    };

    setupCombo (trackBox, "Pista MIDI");
    setupCombo (snapBox, "Snap");
    setupCombo (scaleBox, "Escala");
    setupCombo (rootBox, "Tónica");

    trackBox.onChange = [this]
    {
        refreshFromPattern();
    };

    snapBox.onChange = [this]
    {
        const int i = juce::jlimit (0, kNumSnaps - 1, snapBox.getSelectedId() - 1);
        state.root.setProperty (IDs::snapDiv, snapDivs[i], nullptr);
        applyViewState();
    };

    scaleBox.onChange = [this]
    {
        state.root.setProperty (IDs::scaleId, scaleBox.getText(), nullptr);
        applyViewState();
        refreshGhosts();
    };

    rootBox.onChange = [this]
    {
        state.root.setProperty (IDs::scaleRoot, rootBox.getSelectedId() - 1, nullptr);
        applyViewState();
    };

    auto setupTool = [this] (juce::TextButton& b, PianoRoll::Tool t)
    {
        b.setClickingTogglesState (true);
        b.onClick = [this, t]
        {
            roll.setTool (t);
            drawButton.setToggleState   (t == PianoRoll::Tool::draw,   juce::dontSendNotification);
            selectButton.setToggleState (t == PianoRoll::Tool::select, juce::dontSendNotification);
            eraseButton.setToggleState  (t == PianoRoll::Tool::erase,  juce::dontSendNotification);
        };
        addAndMakeVisible (b);
    };

    setupTool (drawButton,   PianoRoll::Tool::draw);
    setupTool (selectButton, PianoRoll::Tool::select);
    setupTool (eraseButton,  PianoRoll::Tool::erase);
    drawButton.setToggleState (true, juce::dontSendNotification);

    zoomOutButton.onClick = [this]
    {
        int idx = 2;
        for (int i = 0; i < kNumZooms; ++i)
            if (std::abs (zooms[i] - roll.getZoom()) < 0.01f)
                idx = i;
        roll.setZoom (zooms[juce::jmax (0, idx - 1)]);
        resized();
    };

    zoomInButton.onClick = [this]
    {
        int idx = 2;
        for (int i = 0; i < kNumZooms; ++i)
            if (std::abs (zooms[i] - roll.getZoom()) < 0.01f)
                idx = i;
        roll.setZoom (zooms[juce::jmin (kNumZooms - 1, idx + 1)]);
        resized();
    };

    addAndMakeVisible (zoomOutButton);
    addAndMakeVisible (zoomInButton);

    infoLabel.setFont (Theme::font (11.5f, true));
    infoLabel.setColour (juce::Label::textColourId, Theme::accent());
    infoLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (infoLabel);

    keyboard.onKey = [this] (int pitch, bool down)
    {
        const int slot = targetSlot();
        if (slot < 0)
            return;

        if (down) engine.previewNoteOn  (slot, pitch);
        else      engine.previewNoteOff (slot, pitch);
    };

    roll.onPreviewKey = [this] (int pitch, bool down)
    {
        const int slot = targetSlot();
        if (slot < 0)
            return;

        if (down) engine.previewNoteOn  (slot, pitch);
        else      engine.previewNoteOff (slot, pitch);
    };

    rollViewport.setViewedComponent (&roll, false);
    keyViewport.setViewedComponent (&keyboard, false);
    velViewport.setViewedComponent (&velocityLane, false);
    addAndMakeVisible (rollViewport);
    addAndMakeVisible (keyViewport);
    addAndMakeVisible (velViewport);

    roll.onContentChanged = [this] { velocityLane.repaint(); };

    state.root.addListener (this);

    scaleBox.addItemList (Scales::names(), 1);
    rootBox.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);

    for (int i = 0; i < kNumSnaps; ++i)
        snapBox.addItem (snapNames[i], i + 1);

    refreshTracksCombo();
    applyViewState();
    refreshFromPattern();

    startTimerHz (20);
}

MidiEditorPanel::~MidiEditorPanel()
{
    stopTimer();
    state.root.removeListener (this);
    rollViewport.setViewedComponent (nullptr, false);
    keyViewport.setViewedComponent (nullptr, false);
    velViewport.setViewedComponent (nullptr, false);
}

void MidiEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void MidiEditorPanel::resized()
{
    auto r = getLocalBounds();

    auto tb = r.removeFromTop (kToolbarH).reduced (6, 4);

    trackBox.setBounds (tb.removeFromLeft (140));
    tb.removeFromLeft (4);
    snapBox.setBounds (tb.removeFromLeft (64));
    tb.removeFromLeft (4);
    drawButton.setBounds (tb.removeFromLeft (52));
    tb.removeFromLeft (2);
    selectButton.setBounds (tb.removeFromLeft (56));
    tb.removeFromLeft (2);
    eraseButton.setBounds (tb.removeFromLeft (48));
    tb.removeFromLeft (8);
    rootBox.setBounds (tb.removeFromLeft (48));
    tb.removeFromLeft (4);
    scaleBox.setBounds (tb.removeFromLeft (120));
    tb.removeFromLeft (4);
    zoomOutButton.setBounds (tb.removeFromLeft (26));
    tb.removeFromLeft (2);
    zoomInButton.setBounds (tb.removeFromLeft (26));
    infoLabel.setBounds (tb);

    auto velArea = r.removeFromBottom (kVelH);
    velViewport.setScrollBarsShown (false, true, false, false);
    velViewport.getViewedComponent()->setBounds (0, 0, roll.contentWidth(), kVelH);
    velViewport.setBounds (velArea.withTrimmedLeft (kKeyW));

    keyViewport.setBounds (r.removeFromLeft (kKeyW));
    keyViewport.setScrollBarsShown (true, false, false, false);
    keyViewport.getViewedComponent()->setBounds (0, 0, kKeyW, roll.contentHeight());

    rollViewport.setBounds (r);
    roll.setSize (roll.contentWidth(), roll.contentHeight());

    rollViewport.setViewPosition (rollViewport.getViewPosition());
}

void MidiEditorPanel::timerCallback()
{
    const auto pos = playheadBeats();

    if (std::abs (pos - lastPlayhead) > 1.0e-9)
    {
        lastPlayhead = pos;
        roll.setPlayhead (pos);
    }
}

void MidiEditorPanel::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == patternsTree)
    {
        refreshTracksCombo();
        refreshFromPattern();
    }
    else if (parent.getType() == juce::Identifier (IDs::pattern))
    {
        velocityLane.repaint();
    }
    else if (parent.getType() == juce::Identifier (IDs::track))
    {
        refreshGhosts();
    }
}

void MidiEditorPanel::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == patternsTree)
    {
        refreshTracksCombo();
        refreshFromPattern();
    }
    else if (parent.getType() == juce::Identifier (IDs::pattern))
    {
        velocityLane.repaint();
    }
    else if (parent.getType() == juce::Identifier (IDs::track))
    {
        refreshGhosts();
    }
}

void MidiEditorPanel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    if (tree == state.root && prop == juce::Identifier (IDs::activePattern))
        refreshFromPattern();
    else if (tree.getType() == juce::Identifier (IDs::clip))
        refreshGhosts();
}

void MidiEditorPanel::refreshTracksCombo()
{
    ignoreUiCallbacks = true;

    const int previous = trackBox.getSelectedId();
    trackBox.clear (juce::dontSendNotification);

    auto tracks = trackList.getTracks();
    int items = 0;

    for (int i = 0; i < tracks.getNumChildren(); ++i)
    {
        auto t = tracks.getChild (i);
        if (t.getProperty (IDs::type).toString() != "midi")
            continue;

        trackBox.addItem (t.getProperty (IDs::name).toString(), i + 1);
        ++items;
    }

    if (items == 0)
    {
        trackBox.addItem ("(sin pistas MIDI)", 9999);
        trackBox.setSelectedId (9999, juce::dontSendNotification);
    }
    else if (previous >= 1 && previous <= tracks.getNumChildren())
    {
        trackBox.setSelectedId (previous, juce::dontSendNotification);
    }
    else
    {
        trackBox.setSelectedId (1, juce::dontSendNotification);
    }

    ignoreUiCallbacks = false;
}

void MidiEditorPanel::refreshFromPattern()
{
    auto pattern = library.activePattern();

    if (! pattern.isValid() && library.patternCount() > 0)
    {
        library.setActivePatternUid (library.uidOf (library.patterns().getChild (0)));
        return;
    }

    roll.setPattern (pattern);

    if (auto t = targetTrack(); t.isValid())
        roll.setTrackColour (juce::Colour ((juce::uint32) (juce::int64) t.getProperty (IDs::colour, (juce::int64) 0xff4fc3f7)));

    const auto name = pattern.getProperty (IDs::name).toString();
    infoLabel.setText (name.isNotEmpty() ? name : "—", juce::dontSendNotification);

    refreshGhosts();
    velocityLane.repaint();
}

void MidiEditorPanel::refreshGhosts()
{
    std::vector<PianoRoll::Ghost> out;

    auto active = library.activePattern();
    if (active.isValid())
    {
        const int activeUid = library.uidOf (active);
        const double len = library.patternLengthBeats (active);

        double anchor = 0.0;
        bool anchorFound = false;

        auto tracks = library.tracks();
        for (int i = 0; i < tracks.getNumChildren(); ++i)
        {
            auto track = tracks.getChild (i);
            for (int c = 0; c < track.getNumChildren(); ++c)
            {
                auto clip = track.getChild (c);
                if (clip.getType() != juce::Identifier (IDs::clip))
                    continue;

                if ((int) clip.getProperty (IDs::patternUid, -1) != activeUid)
                    continue;

                anchor = (double) clip.getProperty (IDs::start, 0.0);
                anchorFound = true;
                break;
            }
            if (anchorFound)
                break;
        }

        for (int i = 0; i < tracks.getNumChildren(); ++i)
        {
            auto track = tracks.getChild (i);
            if (track.getProperty (IDs::type).toString() != "midi")
                continue;

            for (int c = 0; c < track.getNumChildren(); ++c)
            {
                auto clip = track.getChild (c);
                if (clip.getType() != juce::Identifier (IDs::clip))
                    continue;

                const int puid = (int) clip.getProperty (IDs::patternUid, -1);
                auto pat = library.patternForUid (puid);
                if (! pat.isValid() || pat == active)
                    continue;

                const double clipStart = (double) clip.getProperty (IDs::start, 0.0);
                const double shift = clipStart - anchor;

                for (int n = 0; n < pat.getNumChildren(); ++n)
                {
                    auto note = pat.getChild (n);
                    if (note.getType() != juce::Identifier (IDs::note))
                        continue;

                    const double t = (double) note.getProperty (IDs::start, 0.0) + shift;
                    if (t < -len || t >= 2.0 * len)
                        continue;

                    out.push_back ({ t, (double) note.getProperty (IDs::duration, 0.25),
                                     (int) note.getProperty (IDs::pitch, 60) });
                }
            }
        }
    }

    roll.setGhosts (std::move (out));
}

void MidiEditorPanel::applyViewState()
{
    ignoreUiCallbacks = true;

    const double snap = (double) state.root.getProperty (IDs::snapDiv, 0.25);
    int snapIdx = 3;
    for (int i = 0; i < kNumSnaps; ++i)
        if (std::abs (snapDivs[i] - snap) < 1.0e-6)
            snapIdx = i;
    snapBox.setSelectedId (snapIdx + 1, juce::dontSendNotification);

    const auto scaleId = state.root.getProperty (IDs::scaleId, "Off").toString();
    scaleBox.setText (scaleId, juce::dontSendNotification);

    rootBox.setSelectedId ((int) state.root.getProperty (IDs::scaleRoot, 0) + 1, juce::dontSendNotification);

    ignoreUiCallbacks = false;

    roll.setSnapBeats (snapDivs[snapIdx]);
    roll.setScale ((int) state.root.getProperty (IDs::scaleRoot, 0),
                   Scales::intervalsFor (scaleId));
}

void MidiEditorPanel::syncScroll (int role, const juce::Rectangle<int>& area)
{
    if (syncingScrollNow)
        return;

    struct SyncGuard
    {
        bool& flag;
        ~SyncGuard() { flag = false; }
    } guard { syncingScrollNow };

    syncingScrollNow = true;

    if (role == 0)
    {
        keyViewport.setViewPosition (0, area.getY());
        velViewport.setViewPosition (area.getX(), 0);
    }
    else if (role == 1)
    {
        auto pos = rollViewport.getViewPosition();
        rollViewport.setViewPosition (pos.getX(), area.getY());
        velViewport.setViewPosition (velViewport.getViewPosition().getX(), 0);
    }
    else
    {
        auto pos = rollViewport.getViewPosition();
        rollViewport.setViewPosition (area.getX(), pos.getY());
        keyViewport.setViewPosition (0, keyViewport.getViewPosition().getY());
    }
}

int MidiEditorPanel::targetSlot() const
{
    const int id = trackBox.getSelectedId() - 1;
    auto tracks = trackList.getTracks();

    if (! juce::isPositiveAndBelow (id, tracks.getNumChildren()))
        return -1;

    auto t = tracks.getChild (id);
    if (t.getProperty (IDs::type).toString() != "midi")
        return -1;

    return (int) t.getProperty (IDs::mixerSlot, -1);
}

juce::ValueTree MidiEditorPanel::targetTrack() const
{
    const int id = trackBox.getSelectedId() - 1;
    auto tracks = trackList.getTracks();

    if (! juce::isPositiveAndBelow (id, tracks.getNumChildren()))
        return {};

    return tracks.getChild (id);
}

double MidiEditorPanel::playheadBeats() const
{
    auto pattern = library.activePattern();
    if (! pattern.isValid())
        return -1.0;

    const double pos = engine.positionBeats();
    const int uid = library.uidOf (pattern);

    auto tracks = library.tracks();
    for (int i = 0; i < tracks.getNumChildren(); ++i)
    {
        auto track = tracks.getChild (i);
        for (int c = 0; c < track.getNumChildren(); ++c)
        {
            auto clip = track.getChild (c);
            if (clip.getType() != juce::Identifier (IDs::clip))
                continue;

            if ((int) clip.getProperty (IDs::patternUid, -1) != uid)
                continue;

            const double start = (double) clip.getProperty (IDs::start, 0.0);
            const double len   = (double) clip.getProperty (IDs::duration, 4.0);

            if (pos >= start && pos < start + len)
                return pos - start;
        }
    }

    return -1.0;
}
