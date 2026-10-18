#include "MainWindow.h"
#include "Theme.h"

MainWindow::MainWindow (ProjectState& stateToUse, AudioEngine& engineToUse, TrackList& trackList)
    : DocumentWindow ("MiPropioDaw", Theme::window(), DocumentWindow::allButtons),
      state (stateToUse),
      engine (engineToUse)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (1024, 640, 20000, 12000);

    layout = new MainLayout (state, engine, trackList);
    setContentOwned (layout, true);

    // atajos de teclado (se propagan desde el componente con foco)
    addKeyListener (this);
    setWantsKeyboardFocus (true);

    auto& transport = layout->getTransportBar();
    transport.onSave           = [this] { saveProject (false); };
    transport.onOpen           = [this] { openProject(); };
    transport.onAudioSettings  = [this] { showAudioSettings(); };

    state.onModifiedChanged = [this] { refreshTitle(); };
    state.onProjectLoaded   = [this] { refreshTitle(); };
    refreshTitle();

    centreWithSize (1440, 900);
    setVisible (true);
}

MainWindow::~MainWindow()
{
    state.onModifiedChanged = nullptr;
    state.onProjectLoaded = nullptr;
    removeKeyListener (this);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

bool MainWindow::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    const bool cmd = key.getModifiers().isCommandDown();
    const bool shift = key.getModifiers().isShiftDown();
    const auto code = key.getKeyCode();

    if (cmd && (code == 's' || code == 'S'))
    {
        saveProject (false);
        return true;
    }

    if (cmd && (code == 'o' || code == 'O'))
    {
        openProject();
        return true;
    }

    if (cmd && ! shift && (code == 'z' || code == 'Z'))
    {
        state.performUndo();
        return true;
    }

    if ((cmd && (code == 'y' || code == 'Y')) || (cmd && shift && (code == 'z' || code == 'Z')))
    {
        state.performRedo();
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        engine.togglePlay();
        return true;
    }

    return false;
}

void MainWindow::saveProject (bool forceChooseFile)
{
    if (! forceChooseFile && state.currentFile != juce::File())
    {
        if (state.save (state.currentFile))
            refreshTitle();
        return;
    }

    auto defaultDir = state.currentFile != juce::File()
                          ? state.currentFile
                          : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                .getChildFile ("Untitled.dawproj");

    fileChooser = std::make_unique<juce::FileChooser> ("Save project", defaultDir, "*.dawproj");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File())
                                      return;

                                  if (state.save (file))
                                      refreshTitle();
                              });
}

void MainWindow::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Open project",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.dawproj");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File())
                                      return;

                                  if (! state.load (file))
                                  {
                                      juce::AlertWindow::showAsync (
                                          juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Open failed")
                                              .withMessage ("No se pudo abrir el proyecto:\n" + file.getFullPathName())
                                              .withButton ("OK")
                                              .withAssociatedComponent (this),
                                          [] (int) {});
                                  }
                              });
}

void MainWindow::showAudioSettings()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Audio Settings";
    options.dialogBackgroundColour = Theme::window();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;

    options.content.setOwned (new juce::AudioDeviceSelectorComponent (
        engine.deviceManager(), 0, 8, 0, 8,
        false,          // MIDI entradas
        false,          // MIDI salidas
        true,           // pares estéreo
        false));        // avanzado oculto

    options.content->setSize (560, 500);
    options.launchAsync();
}

void MainWindow::refreshTitle()
{
    setName (state.projectName() + (state.isModified() ? " *" : "") + " - MiPropioDaw");
}
