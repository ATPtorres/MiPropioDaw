#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../model/TrackList.h"
#include "../../model/PatternLibrary.h"
#include "../../audio/AudioEngine.h"

class SessionView : public juce::Component,
                    private juce::ValueTree::Listener,
                    private juce::Timer
{
public:
    SessionView (ProjectState& state, AudioEngine& engine, TrackList& trackList);
    ~SessionView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class ClipCell : public juce::Component
    {
    public:
        ClipCell (SessionView& ownerToUse, juce::ValueTree trackTree, int sceneUidToUse, bool isMidiTrack);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;

    private:
        juce::ValueTree clipTree() const;
        int slotIndex() const;

        SessionView& owner;
        juce::ValueTree track;
        int sceneUid = -1;
        bool midi = false;
    };

    class SceneRow : public juce::Component,
                     private juce::ValueTree::Listener
    {
    public:
        SceneRow (SessionView& ownerToUse, juce::ValueTree sceneTree);
        ~SceneRow() override;

        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;
        void refreshName();

        SessionView& owner;
        juce::ValueTree scene;

        juce::TextButton launchButton { ">", {} };
        juce::Label nameLabel;
        juce::TextButton deleteButton { "x", {} };
    };

    void timerCallback() override;

    void rebuildGrid();
    void scheduleRebuild();
    void layoutGrid();
    void refreshTrackLabels();
    void refreshQuantize();
    float selectedQuantize() const;
    void launchScene (const juce::ValueTree& scene);

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    ProjectState& state;
    AudioEngine& engine;
    TrackList& trackList;
    PatternLibrary library;

    juce::ValueTree tracksTree;
    juce::ValueTree sessionTree;

    juce::Label titleLabel { {}, "SESSION" };
    juce::Label launchLabel { {}, "LAUNCH" };
    juce::ComboBox quantizeBox;
    juce::TextButton stopAllButton { "Stop all", {} };
    juce::Label hintLabel { {}, "Clic = crear con el patrón activo · clic en clip = lanzar · clic en clip sonando = parar · clic derecho = borrar" };

    juce::Viewport viewport;
    juce::Component content;

    std::vector<std::unique_ptr<ClipCell>> cells;
    std::vector<std::unique_ptr<SceneRow>> sceneRows;
    std::vector<std::unique_ptr<juce::Label>> trackLabels;
    std::vector<std::unique_ptr<juce::TextButton>> trackStopButtons;
    std::unique_ptr<juce::TextButton> addSceneButton;

    bool ignoreQuantizeCallback = false;
    bool rebuildPending = false;

    static constexpr int kToolbarH = 34;
    static constexpr int kHeaderH  = 44;
    static constexpr int kSceneColW = 136;
    static constexpr int kCellW    = 120;
    static constexpr int kCellH    = 56;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionView)
};
