#include "MixerView.h"
#include "../Theme.h"
#include "../../core/IDs.h"

MixerView::MixerView (ProjectState& stateToUse, AudioEngine& engineToUse, TrackList& list)
    : state (stateToUse), engine (engineToUse), tracksTree (list.getTracks())
{
    viewport.setViewedComponent (&stripHolder, false);
    viewport.setScrollBarsShown (false, true, false, false);
    viewport.setScrollOnDragEnabled (true);
    addAndMakeVisible (viewport);

    masterStrip = std::make_unique<MixerStrip> (state.mixer(), true, state, engine);
    addAndMakeVisible (*masterStrip);

    tracksTree.addListener (this);
    rebuildStrips();
    startTimerHz (25);
}

MixerView::~MixerView()
{
    stopTimer();
    tracksTree.removeListener (this);
    strips.clear();
    masterStrip.reset();
    viewport.setViewedComponent (nullptr, false);
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::text());
    g.setFont (Theme::font (11.0f, true));
    g.drawText ("MIXER", 10, 6, 80, 16, juce::Justification::centredLeft);

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    if (strips.empty())
    {
        g.setColour (Theme::textDim());
        g.setFont (Theme::font (12.0f));
        g.drawText ("Sin canales — crea pistas en la vista de arreglo",
                    getLocalBounds().removeFromTop (getHeight()).reduced (0, 40),
                    juce::Justification::centred);
    }
}

void MixerView::resized()
{
    auto r = getLocalBounds().reduced (0, 26);
    r.removeFromTop (4);

    auto masterArea = r.removeFromRight (kStripWidth);
    masterStrip->setBounds (masterArea);

    r.removeFromRight (6);
    viewport.setBounds (r.reduced (4, 0));

    layoutStrips();
}

void MixerView::rebuildStrips()
{
    strips.clear();

    for (int i = 0; i < tracksTree.getNumChildren(); ++i)
    {
        auto strip = std::make_unique<MixerStrip> (tracksTree.getChild (i), state, engine);
        stripHolder.addAndMakeVisible (*strip);
        strips.push_back (std::move (strip));
    }

    layoutStrips();
    repaint();
}

void MixerView::layoutStrips()
{
    const auto h = juce::jmax (0, viewport.getHeight() - 4);

    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds ((int) i * kStripWidth, 2, kStripWidth - 2, h);

    stripHolder.setSize (juce::jmax (viewport.getWidth(), (int) strips.size() * kStripWidth + 8),
                         viewport.getHeight());
}

void MixerView::timerCallback()
{
    const double dt = 1.0 / 25.0;

    for (auto& strip : strips)
    {
        strip->tickVU (dt);
        strip->refreshMeter();
    }

    masterStrip->tickVU (dt);
    masterStrip->refreshMeter();
}

void MixerView::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == tracksTree)
        rebuildStrips();
}

void MixerView::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == tracksTree)
        rebuildStrips();
}
