#pragma once

#include <JuceHeader.h>

// Panel dividido con un divisor arrastrable (vertical u horizontal).
class SplitPane : public juce::Component
{
public:
    SplitPane (bool horizontalDivider,
               float initialRatio,
               juce::Component* firstComponent,
               juce::Component* secondComponent,
               float minFirstPx,
               float minSecondPx);

    void resized() override;
    void paint (juce::Graphics&) override;

    float getRatio() const noexcept { return ratio; }
    void setRatio (float newRatio);

private:
    class Divider : public juce::Component
    {
    public:
        explicit Divider (SplitPane& owner) : splitPane (owner) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        SplitPane& splitPane;
        bool hovered = false;
    };

    void dragTo (juce::Point<int> localPosition);

    bool horizontal;
    float ratio;
    const float minFirst, minSecond;
    juce::Component* first;
    juce::Component* second;
    Divider divider { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplitPane)
};
