#include "PianoRoll.h"
#include "../../core/IDs.h"

PianoRoll::PianoRoll (ProjectState& stateToUse, PatternLibrary& libraryToUse)
    : state (stateToUse), library (libraryToUse)
{
    setWantsKeyboardFocus (true);
}

PianoRoll::~PianoRoll()
{
    pattern.removeListener (this);
}

void PianoRoll::setPattern (const juce::ValueTree& patternTree)
{
    if (pattern == patternTree)
        return;

    pattern.removeListener (this);
    pattern = patternTree;
    pattern.addListener (this);

    selectedNote = {};
    dragMode = Drag::none;
    repaint();
}

void PianoRoll::setGhosts (std::vector<Ghost> newGhosts)
{
    ghosts = std::move (newGhosts);
    repaint();
}

void PianoRoll::setTrackColour (juce::Colour c)
{
    trackColour = c;
    repaint();
}

void PianoRoll::setTool (Tool t)
{
    tool = t;
}

void PianoRoll::setSnapBeats (double beats)
{
    snapBeats = juce::jmax (0.0625, beats);
}

void PianoRoll::setScale (int rootPitchClass, const juce::Array<int>& intervals)
{
    scaleRoot = juce::jlimit (0, 11, rootPitchClass);
    scaleIntervals = intervals;
    repaint();
}

void PianoRoll::setZoom (float newZoom)
{
    zoom = juce::jlimit (0.25f, 4.0f, newZoom);
    resized();
    repaint();
}

void PianoRoll::setPlayhead (double beats)
{
    if (std::abs (beats - playheadBeats) < 1.0e-9)
        return;

    playheadBeats = beats;
    repaint();
}

double PianoRoll::contentBeats() const
{
    if (! pattern.isValid())
        return 16.0;

    return library.patternLengthBeats (pattern);
}

int PianoRoll::contentWidth() const
{
    return (int) std::ceil (contentBeats() * pxPerBeat()) + 80;
}

int PianoRoll::contentHeight() const
{
    return 128 * kRowHeight;
}

double PianoRoll::snap (double beats) const
{
    if (snapBeats <= 0.0)
        return beats;

    return std::round (beats / snapBeats) * snapBeats;
}

juce::ValueTree PianoRoll::hitNote (int x, int y) const
{
    if (! pattern.isValid())
        return {};

    const double pos = (double) x / pxPerBeat();
    const int pitch = pitchForY (y);

    for (int i = pattern.getNumChildren() - 1; i >= 0; --i)
    {
        auto n = pattern.getChild (i);
        if (n.getType() != juce::Identifier (IDs::note))
            continue;

        if ((int) n.getProperty (IDs::pitch) != pitch)
            continue;

        const double s = (double) n.getProperty (IDs::start);
        const double d = (double) n.getProperty (IDs::duration);

        if (pos >= s && pos < s + d)
            return n;
    }

    return {};
}

int PianoRoll::velocityForY (int y) const
{
    return juce::jlimit (1, 127, (int) (127.0 * (1.0 - (double) y / (double) juce::jmax (1, getHeight()))));
}
void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    if (! pattern.isValid())
        return;

    if (e.mods.isRightButtonDown() || tool == Tool::erase)
    {
        if (auto n = hitNote (e.x, e.y); n.isValid())
        {
            library.deleteNote (pattern, n);
            if (selectedNote == n)
                selectedNote = {};
        }
        return;
    }

    if (! e.mods.isLeftButtonDown())
        return;

    dragDownX = e.x;
    dragDownY = e.y;

    const double pos = (double) e.x / pxPerBeat();
    const int pitch = juce::jlimit (0, 127, pitchForY (e.y));

    auto existing = hitNote (e.x, e.y);

    if (existing.isValid())
    {
        selectedNote = existing;
        dragNote = existing;
        dragOrigStart = (double) existing.getProperty (IDs::start);
        dragOrigLen   = (double) existing.getProperty (IDs::duration);
        dragOrigPitch = (int) existing.getProperty (IDs::pitch);

        const double endPx = (dragOrigStart + dragOrigLen) * pxPerBeat();
        dragMode = (std::abs ((double) e.x - endPx) <= 6.0) ? Drag::resize : Drag::move;

        state.undo.beginNewTransaction ("Edit note");

        if (onPreviewKey != nullptr)
        {
            onPreviewKey (dragOrigPitch, true);
            previewing = true;
        }
    }
    else if (tool != Tool::select)
    {
        dragMode = Drag::create;

        state.undo.beginNewTransaction ("Add note");
        selectedNote = library.addNote (pattern, pitch, snap (pos), snapBeats, 0.85,
                                        false);

        if (onPreviewKey != nullptr)
        {
            onPreviewKey (pitch, true);
            previewing = true;
        }
    }
    else
    {
        selectedNote = {};
    }

    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == Drag::none || ! dragNote.isValid())
        return;

    if (dragMode == Drag::create)
    {
        const double pos = (double) e.x / pxPerBeat();
        const double raw = pos - snap ((double) dragNote.getProperty (IDs::start));
        state.setProperty (dragNote, IDs::duration,
                           juce::jmax (snapBeats, snap (raw)), "Resize note", 1);
    }
    else if (dragMode == Drag::move)
    {
        const double delta = ((double) (e.x - dragDownX)) / pxPerBeat();
        const int newPitch = juce::jlimit (0, 127, pitchForY (e.y));

        state.setProperty (dragNote, IDs::start,
                           juce::jmax (0.0, snap (dragOrigStart + delta)), "Move note", 1);
        state.setProperty (dragNote, IDs::pitch, newPitch, "Move note", 1);
    }
    else if (dragMode == Drag::resize)
    {
        const double endPos = (double) e.x / pxPerBeat();
        const double newLen = snap (endPos - (double) dragNote.getProperty (IDs::start));

        state.setProperty (dragNote, IDs::duration,
                           juce::jmax (snapBeats, newLen), "Resize note", 1);
    }

    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    dragMode = Drag::none;
    dragNote = {};

    if (previewing && onPreviewKey != nullptr && selectedNote.isValid())
    {
        onPreviewKey ((int) selectedNote.getProperty (IDs::pitch), false);
        previewing = false;
    }
    else if (previewing && onPreviewKey != nullptr)
    {
        onPreviewKey (dragOrigPitch, false);
        previewing = false;
    }
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        setZoom (zoom * (w.deltaY > 0.0f ? 1.1f : 0.9f));
        return;
    }

    Component::mouseWheelMove (e, w);
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    if ((key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
        && selectedNote.isValid() && selectedNote.getParent() == pattern)
    {
        library.deleteNote (pattern, selectedNote);
        selectedNote = {};
        repaint();
        return true;
    }

    return false;
}

void PianoRoll::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent == pattern)
    {
        if (dragMode == Drag::create && child.getType() == juce::Identifier (IDs::note))
            dragNote = child;

        repaint();
        if (onContentChanged)
            onContentChanged();
    }
}

void PianoRoll::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (parent == pattern)
    {
        if (selectedNote == child)
            selectedNote = {};
        if (dragNote == child)
            dragNote = {};

        repaint();
        if (onContentChanged)
            onContentChanged();
    }
}

void PianoRoll::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (tree == pattern || tree.getParent() == pattern)
    {
        repaint();
        if (onContentChanged)
            onContentChanged();
    }
}

void PianoRoll::drawNote (juce::Graphics& g, double start, double len, int pitch,
                          double vel, juce::Colour colour, bool isGhost, bool isSelected)
{
    const float x = (float) (start * pxPerBeat());
    const float w = juce::jmax (3.0f, (float) (len * pxPerBeat()) - 1.0f);
    const float y = (float) yForPitch (pitch) + 1.0f;
    const float h = (float) kRowHeight - 2.0f;

    auto r = juce::Rectangle<float> (x, y, w, h);

    if (isGhost)
    {
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRoundedRectangle (r, 2.0f);
        return;
    }

    g.setColour (colour.withAlpha (0.55f + 0.45f * (float) vel));
    g.fillRoundedRectangle (r, 2.0f);

    const float velW = w * juce::jlimit (0.0f, 1.0f, (float) vel);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.fillRoundedRectangle (juce::Rectangle<float> (x + 1.0f, y + h - 3.0f, juce::jmax (1.0f, velW - 2.0f), 2.0f), 1.0f);

    if (isSelected)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (r, 2.0f, 1.2f);
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    const double ppb = pxPerBeat();

    g.fillAll (Theme::panelAlt());

    const bool scaleOn = ! scaleIntervals.isEmpty();

    // --- filas: teclado implícito + resaltado de escala ----------------
    for (int p = 127; p >= 0; --p)
    {
        const int y = yForPitch (p);
        const int pc = p % 12;
        const bool black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        const bool isC = (pc == 0);

        if (black)
        {
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRect (0, y, w, kRowHeight);
        }

        if (scaleOn && ! Scales::inScale (p, scaleRoot, scaleIntervals))
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRect (0, y, w, kRowHeight);
        }
        else if (isC)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (0, y, w, kRowHeight);
        }

        g.setColour (Theme::gridLine().withAlpha (isC ? 0.9f : 0.45f));
        g.fillRect (0, y + kRowHeight - 1, w, 1);
    }

    // --- rejilla vertical --------------------------------------------
    const int totalBeats = (int) std::ceil (contentBeats()) + 2;

    for (int b = 0; b <= totalBeats; ++b)
    {
        const auto x = (float) ((double) b * ppb);
        if (x > (float) w)
            break;

        const bool isBar = (b % 4) == 0;
        g.setColour (isBar ? Theme::gridLineStrong() : Theme::gridLine());
        g.fillRect (x, 0.0f, isBar ? 1.5f : 1.0f, (float) h);
    }

    // subdivisiones de la rejilla activa
    if (snapBeats < 1.0)
    {
        g.setColour (Theme::gridLine().withAlpha (0.5f));
        const double sub = snapBeats * ppb;
        for (double x = 0.0; x < (double) w; x += sub)
            g.fillRect ((float) x, 0.0f, 1.0f, (float) h);
    }

    // --- notas fantasma ------------------------------------------------
    for (const auto& gh : ghosts)
        drawNote (g, gh.start, gh.length, gh.pitch, 0.5, juce::Colours::grey, true, false);

    // --- notas del patrón ----------------------------------------------
    if (pattern.isValid())
    {
        for (int i = 0; i < pattern.getNumChildren(); ++i)
        {
            auto n = pattern.getChild (i);
            if (n.getType() != juce::Identifier (IDs::note))
                continue;

            drawNote (g,
                      (double) n.getProperty (IDs::start),
                      (double) n.getProperty (IDs::duration),
                      (int) n.getProperty (IDs::pitch),
                      (double) n.getProperty (IDs::velocity, 0.8),
                      trackColour,
                      false,
                      n == selectedNote);
        }
    }

    // --- estado vacío ---------------------------------------------------
    if (pattern.isValid() && pattern.getNumChildren() == 0)
    {
        g.setColour (Theme::textDim().withAlpha (0.7f));
        g.setFont (Theme::font (12.0f));
        g.drawText ("Dibuja notas con la herramienta lápiz · clic derecho = borrar",
                    getLocalBounds().removeFromTop (h / 2).removeFromBottom (40),
                    juce::Justification::centred);
    }
    else if (! pattern.isValid())
    {
        g.setColour (Theme::textDim());
        g.setFont (Theme::font (13.0f));
        g.drawText ("Sin patrón seleccionado — crea uno en la barra lateral",
                    getLocalBounds(), juce::Justification::centred);
    }

    // --- cabezal del editor ---------------------------------------------
    if (playheadBeats >= 0.0)
    {
        const auto x = (float) (playheadBeats * ppb);
        g.setColour (Theme::playhead());
        g.fillRect (x, 0.0f, 1.5f, (float) h);
    }
}
