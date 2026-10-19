#include "ArrangementView.h"
#include "../Theme.h"
#include "../../core/IDs.h"

// ---------------------------------------------------------------------------
// TimelineContent
// ---------------------------------------------------------------------------
void ArrangementView::TimelineContent::paint (juce::Graphics& g)
{
    auto w = (float) getWidth();
    auto h = (float) getHeight();

    g.fillAll (Theme::panelAlt());

    const auto sig = owner.timeSigNum();
    const auto px  = (float) Theme::pxPerBeat;

    // --- rejilla vertical (pulsos y compases) ------------------------
    const int totalBeats = (int) (w / px) + 1;

    for (int b = 0; b <= totalBeats; ++b)
    {
        const auto x = (float) b * px;
        const bool isBar = (b % sig) == 0;

        g.setColour (isBar ? Theme::gridLineStrong() : Theme::gridLine());
        g.fillRect (x, 0.0f, isBar ? 1.5f : 1.0f, h);

        if (isBar)
        {
            g.setColour (Theme::textDim().withAlpha (0.75f));
            g.setFont (Theme::font (9.5f));
            g.drawText (juce::String (b / sig + 1),
                        (int) x + 4, 3, 50, 12, juce::Justification::centredLeft);
        }
    }

    // --- separadores de fila ----------------------------------------
    const int rowH = Theme::trackRowHeight;
    for (int i = 0; i * rowH < (int) h; ++i)
    {
        const auto y = i * rowH;
        g.setColour (Theme::border());
        g.fillRect (0, y, (int) w, 1);
    }

    // --- clips MIDI -----------------------------------------------------
    {
        const int clipRowH = Theme::trackRowHeight;

        for (int i = 0; i < owner.tracksTree.getNumChildren(); ++i)
        {
            auto track = owner.tracksTree.getChild (i);
            const auto baseY = (float) (i * clipRowH);

            const auto colour = juce::Colour ((juce::uint32) (juce::int64) track.getProperty (IDs::colour, (juce::int64) 0xff4fc3f7));

            for (int c = 0; c < track.getNumChildren(); ++c)
            {
                auto clip = track.getChild (c);
                if (clip.getType() != juce::Identifier (IDs::clip))
                    continue;

                const auto start = (double) clip.getProperty (IDs::start, 0.0);
                const auto len   = (double) clip.getProperty (IDs::duration, 4.0);

                const auto cx = (float) (start * px);
                const auto cw = juce::jmax (6.0f, (float) (len * px) - 2.0f);
                const auto rect = juce::Rectangle<float> (cx, baseY + 4.0f, cw, (float) clipRowH - 9.0f);

                auto pat = owner.library.patternForUid ((int) clip.getProperty (IDs::patternUid, -1));
                const auto patName = pat.getProperty (IDs::name, "?").toString();

                g.setColour (colour.withAlpha (0.75f));
                g.fillRoundedRectangle (rect, 4.0f);

                g.setColour (colour.brighter (0.5f));
                g.fillRoundedRectangle (rect.withWidth (4.0f).reduced (1.0f, 3.0f), 2.0f);

                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.setFont (Theme::font (11.0f, true));
                g.drawText (patName, rect.reduced (8.0f, 0.0f), juce::Justification::centredLeft, true);

                g.setColour (colour.brighter (0.2f));
                g.drawRoundedRectangle (rect, 4.0f, 1.0f);
            }
        }
    }

    // --- estado vacío ------------------------------------------------
    if (owner.headers.empty())
    {
        g.setColour (Theme::textDim());
        g.setFont (Theme::font (13.0f));
        g.drawText ("Sin pistas — usa  + Audio  /  + MIDI  para empezar",
                    getLocalBounds().reduced (0, 40), juce::Justification::centred);
    }

    // --- cabezal de reproducción -------------------------------------
    const auto playX = (float) (owner.engine.positionBeats() * px);

    if (playX >= 0.0f && playX <= w)
    {
        g.setColour (Theme::playhead());
        g.fillRect (playX, 0.0f, 1.5f, h);

        juce::Path triangle;
        triangle.startNewSubPath (playX - 5.0f, 0.0f);
        triangle.lineTo (playX + 5.0f, 0.0f);
        triangle.lineTo (playX, 7.0f);
        triangle.closeSubPath();
        g.fillPath (triangle);
    }
}

void ArrangementView::TimelineContent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        auto hit = owner.hitClip (e.x, e.y);
        if (hit.isValid())
            owner.library.deleteClip (hit.track, hit.clip);

        return;
    }

    if (! e.mods.isLeftButtonDown())
        return;

    auto hit = owner.hitClip (e.x, e.y);

    if (hit.isValid())
    {
        owner.dragClip = hit.clip;
        owner.dragClipTrack = hit.track;
        owner.dragDownX = e.x;
        owner.dragOrigStart = (double) hit.clip.getProperty (IDs::start, 0.0);
        owner.dragOrigLen   = (double) hit.clip.getProperty (IDs::duration, 4.0);
        owner.clipDrag = hit.rightEdge ? ClipDrag::resize : ClipDrag::move;

        owner.state.undo.beginNewTransaction ("Edit clip");
        return;
    }

    const int trackIndex = owner.trackIndexAtY (e.y);
    auto active = owner.library.activePattern();

    if (juce::isPositiveAndBelow (trackIndex, owner.tracksTree.getNumChildren())
        && active.isValid())
    {
        auto track = owner.tracksTree.getChild (trackIndex);
        if (track.getProperty (IDs::type).toString() == "midi")
        {
            const double start = snapBeat ((double) e.x / (double) Theme::pxPerBeat);
            auto clip = owner.library.addClip (track, owner.library.uidOf (active), start,
                                               owner.library.patternLengthBeats (active));

            if (clip.isValid())
            {
                owner.dragClip = clip;
                owner.dragClipTrack = track;
                owner.dragDownX = e.x;
                owner.dragOrigStart = start;
                owner.dragOrigLen = (double) clip.getProperty (IDs::duration, 4.0);
                owner.clipDrag = ClipDrag::move;
            }
            return;
        }
    }

    const double beats = (double) e.x / (double) Theme::pxPerBeat;
    owner.engine.setPlayheadBeats (beats);
    repaint();
}

void ArrangementView::TimelineContent::mouseDrag (const juce::MouseEvent& e)
{
    if (owner.clipDrag == ClipDrag::none || ! owner.dragClip.isValid())
        return;

    if (owner.clipDrag == ClipDrag::move)
    {
        const double delta = ((double) (e.x - owner.dragDownX)) / (double) Theme::pxPerBeat;
        owner.state.setProperty (owner.dragClip, IDs::start,
                                 juce::jmax (0.0, snapBeat (owner.dragOrigStart + delta)),
                                 "Move clip", 1);
    }
    else
    {
        const double endPos = (double) e.x / (double) Theme::pxPerBeat;
        const double newLen = snapBeat (endPos - (double) owner.dragClip.getProperty (IDs::start, 0.0));
        owner.state.setProperty (owner.dragClip, IDs::duration,
                                 juce::jmax (1.0, newLen), "Resize clip", 1);
    }

    repaint();
}

void ArrangementView::TimelineContent::mouseUp (const juce::MouseEvent&)
{
    owner.clipDrag = ClipDrag::none;
    owner.dragClip = {};
    owner.dragClipTrack = {};
}

double ArrangementView::snapBeat (double beats)
{
    return std::round (beats);
}

int ArrangementView::trackIndexAtY (int y) const
{
    return y / Theme::trackRowHeight;
}

ArrangementView::ClipHit ArrangementView::hitClip (int x, int y) const
{
    ClipHit hit;
    hit.trackIndex = trackIndexAtY (y);

    if (! juce::isPositiveAndBelow (hit.trackIndex, tracksTree.getNumChildren()))
        return hit;

    hit.track = tracksTree.getChild (hit.trackIndex);
    const double beats = (double) x / (double) Theme::pxPerBeat;

    for (int c = hit.track.getNumChildren() - 1; c >= 0; --c)
    {
        auto clip = hit.track.getChild (c);
        if (clip.getType() != juce::Identifier (IDs::clip))
            continue;

        const double start = (double) clip.getProperty (IDs::start, 0.0);
        const double len   = (double) clip.getProperty (IDs::duration, 4.0);

        if (beats >= start && beats <= start + len)
        {
            hit.clip = clip;
            hit.rightEdge = std::abs (beats - (start + len)) * Theme::pxPerBeat <= 6.0;
            return hit;
        }
    }

    return hit;
}

// ---------------------------------------------------------------------------
// TimelineViewport
// ---------------------------------------------------------------------------
void ArrangementView::TimelineViewport::visibleAreaChanged (const juce::Rectangle<int>& newVisibleArea)
{
    owner.headerViewport.setViewPosition (0, newVisibleArea.getY());
}

// ---------------------------------------------------------------------------
// ArrangementView
// ---------------------------------------------------------------------------
ArrangementView::ArrangementView (ProjectState& stateToUse, AudioEngine& engineToUse, TrackList& list)
    : state (stateToUse), engine (engineToUse), trackList (list), library (stateToUse),
      tracksTree (trackList.getTracks()), transportTree (state.transport())
{
    tracksLabel.setFont (Theme::font (11.0f, true));
    tracksLabel.setColour (juce::Label::textColourId, Theme::textDim());
    tracksLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (tracksLabel);

    addAudioButton.onClick = [this] { addTrack ("audio"); };
    addAndMakeVisible (addAudioButton);

    addMidiButton.onClick = [this] { addTrack ("midi"); };
    addAndMakeVisible (addMidiButton);

    hintLabel.setFont (Theme::font (10.5f));
    hintLabel.setColour (juce::Label::textColourId, Theme::textDim());
    hintLabel.setJustificationType (juce::Justification::centredRight);
    hintLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (hintLabel);

    headerViewport.setViewedComponent (&headerColumn, false);
    headerViewport.setScrollBarsShown (false, false, false, false);
    headerViewport.addMouseListener (this, true);
    addAndMakeVisible (headerViewport);

    timelineViewport.setViewedComponent (&timelineContent, false);
    addAndMakeVisible (timelineViewport);

    tracksTree.addListener (this);
    transportTree.addListener (this);

    rebuildHeaders();
    startTimerHz (15);
}

ArrangementView::~ArrangementView()
{
    stopTimer();
    tracksTree.removeListener (this);
    transportTree.removeListener (this);
    headers.clear();
    headerViewport.setViewedComponent (nullptr, false);
    timelineViewport.setViewedComponent (nullptr, false);
    headerViewport.removeMouseListener (this);
}

void ArrangementView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void ArrangementView::resized()
{
    auto r = getLocalBounds();

    auto toolbar = r.removeFromTop (kToolbarH);
    tracksLabel.setBounds (0, 0, Theme::trackHeaderWidth, toolbar.getHeight());

    auto tools = toolbar.withX (Theme::trackHeaderWidth + 8);
    addAudioButton.setBounds (tools.removeFromLeft (74).reduced (0, 5));
    tools.removeFromLeft (6);
    addMidiButton.setBounds (tools.removeFromLeft (74).reduced (0, 5));
    hintLabel.setBounds (toolbar);

    headerViewport.setBounds (r.removeFromLeft (Theme::trackHeaderWidth));
    timelineViewport.setBounds (r);

    layoutContents();
}

void ArrangementView::layoutContents()
{
    const auto hw = juce::jmax (0, headerViewport.getWidth() - 6);
    headerColumn.setSize (headerViewport.getWidth(), contentHeight());

    for (size_t i = 0; i < headers.size(); ++i)
        headers[i]->setBounds (0, (int) i * Theme::trackRowHeight, hw, Theme::trackRowHeight - 1);

    timelineContent.setSize (timelineWidth(), contentHeight());
}

void ArrangementView::rebuildHeaders()
{
    headers.clear();

    for (int i = 0; i < tracksTree.getNumChildren(); ++i)
    {
        auto header = std::make_unique<TrackHeader> (tracksTree.getChild (i), state, trackList, engine);
        headerColumn.addAndMakeVisible (*header);
        headers.push_back (std::move (header));
    }

    layoutContents();
    timelineContent.repaint();
}

int ArrangementView::timeSigNum() const
{
    return juce::jmax (1, (int) transportTree.getProperty (IDs::timeSigNum, 4));
}

int ArrangementView::timelineWidth() const
{
    return Theme::defaultBars * timeSigNum() * Theme::pxPerBeat + 400;
}

int ArrangementView::contentHeight() const
{
    return juce::jmax (timelineViewport.getHeight(),
                       (int) headers.size() * Theme::trackRowHeight + 40);
}

void ArrangementView::addTrack (const juce::String& type)
{
    if (! trackList.addTrack (type).isValid())
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Track limit")
                .withMessage ("No quedan huecos de mixer (máximo " +
                              juce::String (AudioEngine::kMaxSlots) + " pistas por sesión).")
                .withButton ("OK")
                .withAssociatedComponent (this),
            [] (int) {});
    }
}

void ArrangementView::timerCallback()
{
    timelineContent.repaint();
}

void ArrangementView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused (e);

    auto pos = timelineViewport.getViewPosition();
    const auto dy = (int) (wheel.deltaY * 3.0f * (float) Theme::trackRowHeight * 2.0f);
    timelineViewport.setViewPosition (pos.x, juce::jmax (0, pos.y - dy));

    // sincroniza siempre la columna de cabeceras con el timeline
    headerViewport.setViewPosition (0, timelineViewport.getViewPosition().getY());
}

void ArrangementView::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == tracksTree)
        rebuildHeaders();
    else if (parent.getType() == juce::Identifier (IDs::track))
        timelineContent.repaint();
}

void ArrangementView::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == tracksTree)
        rebuildHeaders();
    else if (parent.getType() == juce::Identifier (IDs::track))
        timelineContent.repaint();
}

void ArrangementView::valueTreePropertyChanged (juce::ValueTree& changedTree, const juce::Identifier& prop)
{
    if (changedTree == transportTree && prop == juce::Identifier (IDs::timeSigNum))
        layoutContents();
    else if (changedTree.getType() == juce::Identifier (IDs::clip))
        timelineContent.repaint();
}
