#include "SplitPane.h"
#include "Theme.h"

namespace
{
    constexpr int kThickness = (int) Theme::splitterWidth;
}

SplitPane::SplitPane (bool isHorizontal, float initialRatio,
                      juce::Component* firstComponent, juce::Component* secondComponent,
                      float minFirstPx, float minSecondPx)
    : horizontal (isHorizontal),
      ratio (juce::jlimit (0.02f, 0.98f, initialRatio)),
      minFirst (minFirstPx),
      minSecond (minSecondPx),
      first (firstComponent),
      second (secondComponent)
{
    addAndMakeVisible (first);
    addAndMakeVisible (second);
    addAndMakeVisible (divider);
}

void SplitPane::setRatio (float newRatio)
{
    ratio = juce::jlimit (0.02f, 0.98f, newRatio);
    resized();
}

void SplitPane::resized()
{
    auto area = getLocalBounds();

    if (area.getWidth() <= kThickness || area.getHeight() <= kThickness)
    {
        divider.setBounds (area);
        return;
    }

    if (horizontal)
    {
        const int total = area.getWidth() - kThickness;
        int firstSize = juce::roundToInt ((float) total * ratio);
        firstSize = juce::jlimit ((int) minFirst, juce::jmax ((int) minFirst, total - (int) minSecond), firstSize);

        first->setBounds (area.removeFromLeft (firstSize));
        divider.setBounds (area.removeFromLeft (kThickness));
        second->setBounds (area);
    }
    else
    {
        const int total = area.getHeight() - kThickness;
        int firstSize = juce::roundToInt ((float) total * ratio);
        firstSize = juce::jlimit ((int) minFirst, juce::jmax ((int) minFirst, total - (int) minSecond), firstSize);

        first->setBounds (area.removeFromTop (firstSize));
        divider.setBounds (area.removeFromTop (kThickness));
        second->setBounds (area);
    }
}

void SplitPane::paint (juce::Graphics&)
{
}

void SplitPane::dragTo (juce::Point<int> localPosition)
{
    auto area = getLocalBounds().toFloat();

    if (horizontal)
    {
        const float usable = area.getWidth() - (float) kThickness;
        if (usable > 1.0f)
            setRatio (((float) localPosition.x - (float) kThickness * 0.5f) / usable);
    }
    else
    {
        const float usable = area.getHeight() - (float) kThickness;
        if (usable > 1.0f)
            setRatio (((float) localPosition.y - (float) kThickness * 0.5f) / usable);
    }
}

// ---------------------------------------------------------------------------
void SplitPane::Divider::paint (juce::Graphics& g)
{
    g.setColour (Theme::panel());

    if (hovered)
    {
        g.fillAll();
        g.setColour (Theme::accent().withAlpha (0.5f));

        if (splitPane.horizontal)
            g.fillRect (getLocalBounds().withWidth (1));
        else
            g.fillRect (getLocalBounds().withHeight (1));
    }
    else
    {
        g.setColour (Theme::border());
        g.fillAll();
    }
}

void SplitPane::Divider::mouseDown (const juce::MouseEvent& e)
{
    splitPane.dragTo (e.getEventRelativeTo (&splitPane).getPosition());
}

void SplitPane::Divider::mouseDrag (const juce::MouseEvent& e)
{
    splitPane.dragTo (e.getEventRelativeTo (&splitPane).getPosition());
}

void SplitPane::Divider::mouseEnter (const juce::MouseEvent&)
{
    hovered = true;
    repaint();
    setMouseCursor (splitPane.horizontal ? juce::MouseCursor::LeftRightResizeCursor
                                         : juce::MouseCursor::UpDownResizeCursor);
}

void SplitPane::Divider::mouseExit (const juce::MouseEvent&)
{
    hovered = false;
    repaint();
    setMouseCursor (juce::MouseCursor::NormalCursor);
}
