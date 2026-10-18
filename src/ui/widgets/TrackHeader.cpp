#include "TrackHeader.h"
#include "../Theme.h"
#include "../../core/IDs.h"

// ---------------------------------------------------------------------------
void TrackHeader::ColourSwatch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto colour = juce::Colour ((juce::uint32) (juce::int64) owner.tree.getProperty (IDs::colour, juce::var ((juce::int64) 0xff64b5f6)));

    g.setColour (colour);
    g.fillRoundedRectangle (r, 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void TrackHeader::ColourSwatch::mouseDown (const juce::MouseEvent&)
{
    owner.showColourMenu();
}

// ---------------------------------------------------------------------------
TrackHeader::TrackHeader (juce::ValueTree trackTree, ProjectState& stateToUse,
                          TrackList& list, AudioEngine& engineToUse)
    : tree (std::move (trackTree)),
      state (stateToUse),
      trackList (list),
      engine (engineToUse)
{
    addAndMakeVisible (swatch);

    nameLabel.setInterceptsMouseClicks (false, false);
    nameLabel.setFont (Theme::font (13.0f));
    nameLabel.setEditable (true, false, true);
    nameLabel.onTextChange = [this]
    {
        if (! ignoreUiCallbacks)
            trackList.renameTrack (tree, nameLabel.getText().trim());
    };
    addAndMakeVisible (nameLabel);

    rebuildInputCombo();
    inputBox.onChange = [this]
    {
        if (ignoreUiCallbacks)
            return;

        const int value = inputBox.getSelectedId() - 3;
        state.setProperty (tree, IDs::inputChannel, value, "Set track input");
    };
    addAndMakeVisible (inputBox);

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonColourId, Theme::accent2());
    muteButton.onClick = [this]
    {
        if (! ignoreUiCallbacks)
            state.setProperty (tree, IDs::mute, muteButton.getToggleState(), "Mute track");
    };
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonColourId, Theme::danger());
    soloButton.onClick = [this]
    {
        if (! ignoreUiCallbacks)
            state.setProperty (tree, IDs::solo, soloButton.getToggleState(), "Solo track");
    };
    addAndMakeVisible (soloButton);

    deleteButton.setButtonText ("x");
    deleteButton.setColour (juce::TextButton::textColourOffId, Theme::textDim());
    deleteButton.onClick = [this] { trackList.removeTrack (tree); };
    addAndMakeVisible (deleteButton);

    tree.addListener (this);
    refreshFromTree();
}

TrackHeader::~TrackHeader()
{
    tree.removeListener (this);
}

void TrackHeader::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    g.setColour (Theme::textDim().withAlpha (0.6f));
    g.setFont (Theme::font (10.0f));
    g.drawText ("Ch " + juce::String ((int) tree.getProperty (IDs::mixerSlot, 0) + 1),
                getWidth() - 44, 27, 40, 18, juce::Justification::centredRight);
}

void TrackHeader::resized()
{
    auto r = getLocalBounds().reduced (5, 3);

    auto row1 = r.removeFromTop (21);
    auto row2 = r.removeFromTop (20);

    const bool isAudio = tree.getProperty (IDs::type).toString() == "audio";

    deleteButton.setBounds (row1.removeFromRight (18));
    swatch.setBounds (row1.removeFromLeft (14).reduced (0, 3));
    row1.removeFromLeft (5);
    nameLabel.setBounds (row1);

    if (isAudio)
        inputBox.setBounds (row2.removeFromLeft (juce::jmin (130, row2.getWidth() - 60)));

    row2.removeFromLeft (4);
    soloButton.setBounds (row2.removeFromRight (24));
    row2.removeFromRight (3);
    muteButton.setBounds (row2.removeFromRight (24));
}

void TrackHeader::refreshFromTree()
{
    ignoreUiCallbacks = true;

    if (! nameLabel.hasKeyboardFocus (true))
        nameLabel.setText (tree.getProperty (IDs::name), juce::dontSendNotification);

    inputBox.setSelectedId ((int) tree.getProperty (IDs::inputChannel, AudioEngine::kNoInput) + 3,
                            juce::dontSendNotification);
    muteButton.setToggleState ((bool) tree.getProperty (IDs::mute, false), juce::dontSendNotification);
    soloButton.setToggleState ((bool) tree.getProperty (IDs::solo, false), juce::dontSendNotification);

    ignoreUiCallbacks = false;
    swatch.repaint();
    repaint();
}

void TrackHeader::rebuildInputCombo()
{
    inputBox.clear (juce::dontSendNotification);

    inputBox.addItem ("-- input --", AudioEngine::kNoInput + 3);   // -1 + 3 = 2
    inputBox.addItem ("Tone 440Hz", AudioEngine::kTestToneInput + 3); // -2 + 3 = 1

    const int n = engine.numDeviceInputs();
    for (int i = 0; i < n; ++i)
        inputBox.addItem ("In " + juce::String (i + 1), i + 3);

    inputBox.setSelectedId ((int) tree.getProperty (IDs::inputChannel, AudioEngine::kNoInput) + 3,
                            juce::dontSendNotification);
}

void TrackHeader::showColourMenu()
{
    struct ColourGrid : juce::Component
    {
        TrackHeader& owner;
        std::vector<std::unique_ptr<juce::TextButton>> buttons;

        explicit ColourGrid (TrackHeader& o) : owner (o)
        {
            static const juce::uint32 colours[] = {
                0xffe57373, 0xfff06292, 0xffba68c8, 0xff64b5f6, 0xff4dd0e1,
                0xff81c784, 0xffffd54f, 0xffff8a65, 0xffa1887f, 0xff90a4ae
            };

            for (auto c : colours)
            {
                auto* b = new juce::TextButton (juce::String());
                b->setColour (juce::TextButton::buttonColourId, juce::Colour (c));
                b->setColour (juce::TextButton::textColourOffId, juce::Colours::transparentBlack);

                const auto value = (juce::int32) c;
                b->onClick = [this, value]
                {
                    owner.state.setProperty (owner.tree, IDs::colour, value, "Change track colour");
                    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                        box->dismiss();
                };

                buttons.emplace_back (b);
                addAndMakeVisible (*b);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            const int cellW = r.getWidth() / 5;
            const int cellH = r.getHeight() / 2;

            for (size_t i = 0; i < buttons.size(); ++i)
            {
                const int col = (int) i % 5;
                const int row = (int) i / 5;
                buttons[i]->setBounds (r.getX() + col * cellW + 3, r.getY() + row * cellH + 3,
                                       cellW - 6, cellH - 6);
            }
        }
    };

    auto* grid = new ColourGrid (*this);
    grid->setSize (5 * 30 + 12, 2 * 30 + 12);

    juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (grid),
                                            swatch.getScreenBounds(),
                                            swatch.getTopLevelComponent());
}
