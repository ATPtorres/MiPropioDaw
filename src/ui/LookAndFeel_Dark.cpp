#include "LookAndFeel_Dark.h"
#include "Theme.h"

LookAndFeel_Dark::LookAndFeel_Dark()
{
    setColour (juce::ResizableWindow::backgroundColourId, Theme::window());

    setColour (juce::TextButton::buttonColourId,      Theme::buttonOff());
    setColour (juce::TextButton::textColourOffId,     Theme::textDim());
    setColour (juce::TextButton::textColourOnId,      juce::Colours::white);
    setColour (juce::TextEditor::textColourId,        Theme::text());
    setColour (juce::TextEditor::backgroundColourId,  juce::Colour (0xff161616));
    setColour (juce::TextEditor::outlineColourId,     Theme::border());
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent());

    setColour (juce::Label::textColourId,        Theme::text());
    setColour (juce::Label::outlineColourId,     juce::Colours::transparentBlack);
    setColour (juce::Label::backgroundColourId,  juce::Colours::transparentBlack);

    setColour (juce::Slider::thumbColourId,       Theme::accent());
    setColour (juce::Slider::trackColourId,       juce::Colour (0xff3a3a3a));
    setColour (juce::Slider::backgroundColourId,  juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, Theme::text());
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour (juce::ComboBox::backgroundColourId,   juce::Colour (0xff232323));
    setColour (juce::ComboBox::textColourId,         Theme::text());
    setColour (juce::ComboBox::outlineColourId,      Theme::border());
    setColour (juce::ComboBox::arrowColourId,        Theme::textDim());
    setColour (juce::ComboBox::buttonColourId,       juce::Colour (0xff232323));
    setColour (juce::ComboBox::focusedOutlineColourId, Theme::accent());

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e1e1e));
    setColour (juce::PopupMenu::textColourId,       Theme::text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent().withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1e1e1e));

    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId,      juce::Colour (0xff4a4a4a));

    setColour (juce::TabbedComponent::backgroundColourId, Theme::panel());
    setColour (juce::TabbedButtonBar::tabOutlineColourId, Theme::border());
    setColour (juce::TabbedButtonBar::tabTextColourId,    Theme::textDim());
    setColour (juce::TabbedButtonBar::frontTextColourId,  Theme::text());
    setColour (juce::TabbedButtonBar::frontOutlineColourId, Theme::border());

    setColour (juce::ListBox::backgroundColourId, Theme::panel());
    setColour (juce::ListBox::textColourId,       Theme::text());
    setColour (juce::ListBox::outlineColourId,    juce::Colours::transparentBlack);
}

void LookAndFeel_Dark::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    auto colour = on ? button.findColour (juce::TextButton::buttonColourId)
                     : button.findColour (juce::TextButton::buttonColourId, true);

    if (! on)
        colour = Theme::buttonOff();

    if (button.isEnabled() == false)
        colour = colour.withAlpha (0.35f);
    else if (down)
        colour = colour.brighter (0.25f);
    else if (highlighted)
        colour = colour.brighter (0.12f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (on ? juce::Colours::transparentBlack : Theme::border());
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

void LookAndFeel_Dark::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                       bool highlighted, bool)
{
    auto colour = button.findColour (juce::TextButton::textColourOffId);

    if (button.getToggleState() && button.findColour (juce::TextButton::buttonColourId) != Theme::buttonOff())
        colour = juce::Colours::white;

    if (! button.isEnabled())
        colour = colour.withAlpha (0.35f);
    else if (highlighted)
        colour = colour.brighter (0.2f);

    g.setColour (colour);
    g.setFont (Theme::font (12.5f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

void LookAndFeel_Dark::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // flecha
    auto arrowArea = juce::Rectangle<float> ((float) (width - 18), 0.0f, 18.0f, (float) height).reduced (5.0f, (float) height * 0.3f);
    juce::Path path;
    path.startNewSubPath (arrowArea.getX(), arrowArea.getCentreY() - 2.0f);
    path.lineTo (arrowArea.getCentreX(), arrowArea.getCentreY() + 2.0f);
    path.lineTo (arrowArea.getRight(), arrowArea.getCentreY() - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (path, juce::PathStrokeType (1.5f));
}

void LookAndFeel_Dark::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setJustificationType (juce::Justification::centredLeft);
}

void LookAndFeel_Dark::drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g)
{
    g.fillAll (Theme::panel());
    g.setColour (Theme::border());
    g.fillRect (0, bar.getBottom() - 1, bar.getWidth(), 1);
}

void LookAndFeel_Dark::drawTabButton (juce::TabBarButton& tab, juce::Graphics& g,
                                      bool isMouseOverButton, bool)
{
    auto r = tab.getLocalBounds().toFloat();
    const bool active = tab.getToggleState();

    g.fillAll (active ? Theme::panelAlt() : Theme::panel());

    if (isMouseOverButton && ! active)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (r);
    }

    if (active)
    {
        g.setColour (Theme::accent());
        g.fillRect (r.removeFromBottom (2.0f));
    }

    g.setColour (Theme::border());
    g.drawHorizontalLine (tab.getHeight() - 1, 0.0f, (float) tab.getWidth());

    g.setColour (active ? Theme::text() : Theme::textDim());
    g.setFont (Theme::font (12.0f, active));
    g.drawText (tab.getButtonText(), tab.getLocalBounds().reduced (4, 0), juce::Justification::centred);
}

void LookAndFeel_Dark::drawScrollbar (juce::Graphics& g, juce::ScrollBar&,
                                      int x, int y, int width, int height, bool vertical,
                                      int thumbStartPosition, int thumbSize,
                                      bool isMouseOver, bool)
{
    juce::Colour c = isMouseOver ? juce::Colour (0xff5a5a5a) : juce::Colour (0xff3d3d3d);

    if (vertical)
    {
        g.setColour (c);
        g.fillRoundedRectangle ((float) x + 1.0f, (float) (y + thumbStartPosition),
                                (float) width - 2.0f, (float) thumbSize, 2.0f);
    }
    else
    {
        g.setColour (c);
        g.fillRoundedRectangle ((float) (x + thumbStartPosition), (float) y + 1.0f,
                                (float) thumbSize, (float) height - 2.0f, 2.0f);
    }
}
