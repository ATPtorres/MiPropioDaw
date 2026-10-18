#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../model/TrackList.h"
#include "../../audio/AudioEngine.h"
#include "../widgets/MixerStrip.h"

// Consola de mezcla: tiras por pista (scroll horizontal) + canal Master fijo.
// Fase 2: ProChannel analógico por canal (EQ, compresor, saturador, VU).
class MixerView : public juce::Component,
                  private juce::ValueTree::Listener,
                  private juce::Timer
{
public:
    MixerView (ProjectState& state, AudioEngine& engine, TrackList& trackList);
    ~MixerView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildStrips();
    void layoutStrips();

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;

    ProjectState& state;
    AudioEngine& engine;
    juce::ValueTree tracksTree;

    juce::Viewport viewport;
    juce::Component stripHolder;
    std::vector<std::unique_ptr<MixerStrip>> strips;
    std::unique_ptr<MixerStrip> masterStrip;

    static constexpr int kStripWidth = 190;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
};
