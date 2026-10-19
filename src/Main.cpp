#include <JuceHeader.h>

#include "core/ProjectState.h"
#include "model/TrackList.h"
#include "audio/AudioEngine.h"
#include "ui/MainWindow.h"
#include "ui/LookAndFeel_Dark.h"

#ifndef JUCE_APPLICATION_NAME_STRING
 #define JUCE_APPLICATION_NAME_STRING "MiPropioDaw"
#endif
#ifndef JUCE_APPLICATION_VERSION_STRING
 #define JUCE_APPLICATION_VERSION_STRING "0.1.0"
#endif

class MiPropioDawApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "MiPropioDaw"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        lookAndFeel = std::make_unique<LookAndFeel_Dark>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        projectState = std::make_unique<ProjectState>();
        audioEngine  = std::make_unique<AudioEngine>();
        trackList    = std::make_unique<TrackList> (*projectState, *audioEngine);

        // modelo <-> motor
        audioEngine->attachTracks    (trackList->getTracks());
        audioEngine->attachPatterns  (projectState->patterns());
        audioEngine->attachTransport (projectState->transport());
        audioEngine->attachMixer     (projectState->mixer());

        juce::String deviceError;
        audioEngine->initDevice (deviceError);

        mainWindow = std::make_unique<MainWindow> (*projectState, *audioEngine, *trackList);

        if (deviceError.isNotEmpty())
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle ("Audio device")
                    .withMessage ("No se pudo abrir el dispositivo de audio:\n" + deviceError
                                  + "\n\nConfigúralo con el botón Audio... de la barra superior.")
                    .withButton ("OK"),
                [] (int) {});
        }
    }

    void shutdown() override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);

        mainWindow.reset();
        trackList.reset();
        audioEngine.reset();
        projectState.reset();
        lookAndFeel.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<LookAndFeel_Dark> lookAndFeel;
    std::unique_ptr<ProjectState> projectState;
    std::unique_ptr<AudioEngine> audioEngine;
    std::unique_ptr<TrackList> trackList;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (MiPropioDawApplication)
