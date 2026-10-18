#pragma once

#include <JuceHeader.h>

// LookAndFeel oscuro, minimalista y vectorial (sin bitmaps).
class LookAndFeel_Dark : public juce::LookAndFeel_V4
{
public:
    LookAndFeel_Dark();

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&,
                        bool isMouseOverButton, bool isButtonDown) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&,
                        int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;
};
