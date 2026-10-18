#include <JuceHeader.h>
#include <iostream>

#include "../src/core/ProjectState.h"
#include "../src/model/TrackList.h"
#include "../src/audio/AudioEngine.h"
#include "../src/audio/ProChannel.h"
#include "../src/audio/SpectrumAnalyzer.h"
#include "../src/core/IDs.h"

static int gFailures = 0;
static int gChecks = 0;

static void check (bool condition, const juce::String& what)
{
    ++gChecks;
    if (condition)
    {
        std::cout << "  PASS: " << what << "\n";
    }
    else
    {
        ++gFailures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

static void testTrackCreationAndRouting()
{
    std::cout << "[test] creación de pistas y enrutamiento automático\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);

    engine.attachTracks (tracks.getTracks());

    auto audio1 = tracks.addTrack ("audio");
    auto midi1  = tracks.addTrack ("midi");

    check (audio1.isValid(), "pista de audio creada");
    check (midi1.isValid(),  "pista MIDI creada");
    check (tracks.trackCount() == 2, "conteo de pistas == 2");

    const int slotA = (int) audio1.getProperty (IDs::mixerSlot, -1);
    const int slotM = (int) midi1.getProperty (IDs::mixerSlot, -1);

    check (slotA >= 0 && slotM >= 0 && slotA != slotM, "slots de mixer únicos y asignados");
    check (engine.getSlot (slotA).active.load(), "slot de audio activo en el motor");
    check (engine.getSlot (slotM).active.load(), "slot de MIDI activo en el motor");
    check (audio1.getProperty (IDs::type).toString() == "audio", "tipo de pista correcto");
    check (audio1.hasProperty (IDs::inputChannel), "pista de audio con canal de entrada asignado");
}

static void testUndoRedo()
{
    std::cout << "[test] undo/redo de pistas\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);

    engine.attachTracks (tracks.getTracks());

    auto t = tracks.addTrack ("audio");
    const int slot = (int) t.getProperty (IDs::mixerSlot, -1);

    check (engine.getSlot (slot).active.load(), "slot activo tras añadir");

    state.undo.undo();
    check (tracks.trackCount() == 0, "deshacer elimina la pista de la lista");
    check (! engine.getSlot (slot).active.load(), "deshacer desactiva el slot");

    state.undo.redo();
    check (tracks.trackCount() == 1, "rehacer restaura la pista");
    check (engine.getSlot (slot).active.load(), "rehacer reactiva el slot");

    // mute / solo / gain se reflejan en el motor
    auto track = tracks.getTracks().getChild (0);
    state.setProperty (track, IDs::mute, true, "Mute");
    check (engine.getSlot (slot).mute.load(), "mute propagado al motor");

    state.setProperty (track, IDs::gain, -6.0f, "Gain");
    const auto expected = juce::Decibels::decibelsToGain (-6.0f);
    check (std::abs (engine.getSlot (slot).gain.load() - expected) < 1.0e-4f, "gain en dB propagado al motor");

    state.setProperty (track, IDs::pan, -0.5f, "Pan");
    check (std::abs (engine.getSlot (slot).pan.load() + 0.5f) < 1.0e-4f, "paneo propagado al motor");
}

static void testSerializationRoundtrip()
{
    std::cout << "[test] guardar / cargar proyecto\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);
    engine.attachTracks (tracks.getTracks());

    state.setProperty (state.transport(), IDs::bpm, 128.0, "Set tempo");
    tracks.addTrack ("audio");
    tracks.addTrack ("midi");

    auto pattern = juce::ValueTree (IDs::pattern);
    pattern.setProperty (IDs::name, "Chorus", nullptr);
    state.addChild (state.patterns(), pattern, -1, "Add pattern");

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("miproiodaw_test.dawproj");

    check (state.save (file), "archivo guardado");
    check (file.existsAsFile(), "archivo existe en disco");

    ProjectState loaded;
    check (loaded.load (file), "archivo cargado");
    check ((int) loaded.transport().getProperty (IDs::bpm) == 128, "BPM restaurado");
    check (loaded.tracks().getNumChildren() == 2, "2 pistas restauradas");
    check (loaded.patterns().getNumChildren() == 1, "1 patrón restaurado");
    check (loaded.patterns().getChild (0).getProperty (IDs::name).toString() == "Chorus",
           "nombre de patrón restaurado");

    // el motor puede reconectar el proyecto cargado
    AudioEngine engine2;
    engine2.attachTracks (loaded.tracks());
    const int slot = (int) loaded.tracks().getChild (0).getProperty (IDs::mixerSlot, -1);
    check (engine2.getSlot (slot).active.load(), "motor sincronizado con proyecto cargado");

    file.deleteFile();
}

static void testSlotCapacity()
{
    std::cout << "[test] límite de slots\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);
    engine.attachTracks (tracks.getTracks());

    int added = 0;
    while (tracks.addTrack ("audio").isValid())
        ++added;

    check (added == AudioEngine::kMaxSlots, "se crean exactamente " +
                                              juce::String (AudioEngine::kMaxSlots) + " pistas ("
                                              + juce::String (added) + ")");
}

static void testTransportText()
{
    std::cout << "[test] posición del transporte\n";

    AudioEngine engine;
    engine.attachTransport (juce::ValueTree());  // sin árbol: valores por defecto

    engine.setPlayheadBeats (0.0);
    check (engine.positionText() == "001.1.000", "posición inicial 001.1.000");

    engine.setPlayheadBeats (5.5);   // con compás 4/4 → barra 2, pulso 2
    check (engine.positionText().startsWith ("002.2."), "posición barra 2 pulso 2 ("
                                                          + engine.positionText() + ")");
}

// ---------------------------------------------------------------------------
// Tests de DSP — Fase 2
// ---------------------------------------------------------------------------
static void testProChannelEq()
{
    std::cout << "[test] ProChannel: EQ\n";

    ProChannelProcessor pc;
    pc.prepare (48000.0, 256);

    // Señal de prueba: seno 1 kHz, amplitude 0.5
    const int N = 2048;
    std::vector<float> buf (N, 0.0f);
    for (int i = 0; i < N; ++i)
        buf[(size_t) i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) i / 48000.0f);

    // EQ apagado: la señal no cambia
    pc.params.eqEnabled.store (false);
    auto ref = buf;
    pc.process (buf.data(), N);
    float maxDiffOff = 0.0f;
    for (int i = 0; i < N; ++i)
        maxDiffOff = juce::jmax (maxDiffOff, std::abs (buf[(size_t) i] - ref[(size_t) i]));
    check (maxDiffOff < 1.0e-6f, "EQ apagado: señal intacta");

    // EQ encendido con boost de +12 dB en 1 kHz (mid peak)
    pc.params.eqEnabled.store (true);
    pc.params.eqMidGain.store (12.0f);
    pc.params.eqMidFreq.store (1000.0f);
    pc.params.eqMidQ.store (1.0f);
    pc.params.eqLowGain.store (0.0f);
    pc.params.eqHighGain.store (0.0f);

    // reponer señal de referencia
    for (int i = 0; i < N; ++i)
        buf[(size_t) i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) i / 48000.0f);

    pc.process (buf.data(), N);

    // mide RMS de salida vs entrada
    auto rms = [] (const std::vector<float>& v)
    {
        double s = 0.0;
        for (float x : v) s += (double) x * (double) x;
        return (float) std::sqrt (s / (double) v.size());
    };

    const float rmsIn  = rms (ref);
    const float rmsOut = rms (buf);
    const float boostDb = juce::Decibels::gainToDecibels (rmsOut) - juce::Decibels::gainToDecibels (rmsIn);

    // La saturación analógica post-filter recorta el boost, pero debe ser evidente
    check (boostDb > 3.0f,
           "EQ mid +12 dB produce boost real (" + juce::String (boostDb, 1) + " dB)");
}

static void testProChannelCompressor()
{
    std::cout << "[test] ProChannel: compresor\n";

    ProChannelProcessor pc;
    pc.prepare (48000.0, 256);

    // Señal fuerte: seno 0.9 amplitude
    const int N = 4096;
    std::vector<float> buf (N, 0.0f);
    for (int i = 0; i < N; ++i)
        buf[(size_t) i] = 0.9f * std::sin (juce::MathConstants<float>::twoPi * 500.0f * (float) i / 48000.0f);

    auto ref = buf;

    // Compresor: threshold -12 dB, ratio 4:1, sin makeup
    pc.params.compEnabled.store (true);
    pc.params.compThreshold.store (-12.0f);
    pc.params.compRatio.store (4.0f);
    pc.params.compAttack.store (1.0f);
    pc.params.compRelease.store (50.0f);
    pc.params.compMakeup.store (0.0f);

    pc.process (buf.data(), N);

    auto rms = [] (const std::vector<float>& v)
    {
        double s = 0.0;
        for (float x : v) s += (double) x * (double) x;
        return (float) std::sqrt (s / (double) v.size());
    };

    const float rmsIn  = rms (ref);
    const float rmsOut = rms (buf);
    const float changeDb = juce::Decibels::gainToDecibels (rmsOut) - juce::Decibels::gainToDecibels (rmsIn);

    // entrada ~ -1 dBFS, threshold -12 dB → over = 11 dB → GR = 11*(1/4-1) = -8.25 dB
    check (changeDb < -4.0f, "compresor reduce nivel (" + juce::String (changeDb, 1) + " dB)");
    check (pc.grDb.load() < -1.0f, "GR reportado negativo (" + juce::String (pc.grDb.load(), 1) + " dB)");
}

static void testProChannelSaturation()
{
    std::cout << "[test] ProChannel: saturación de tubo\n";

    ProChannelProcessor pc;
    pc.prepare (48000.0, 256);

    // Señal suave: seno 0.1 amplitude
    const int N = 2048;
    std::vector<float> buf (N, 0.0f);
    for (int i = 0; i < N; ++i)
        buf[(size_t) i] = 0.1f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / 48000.0f);

    auto ref = buf;

    // Saturación fuerte: drive 0.8
    pc.params.satEnabled.store (true);
    pc.params.satDrive.store (0.8f);
    pc.params.satBias.store (0.3f);   // asimetría → armónicos pares
    pc.params.satOutput.store (0.0f);

    pc.process (buf.data(), N);

    // La salida debe diferir de la entrada
    float maxDiff = 0.0f;
    for (int i = 0; i < N; ++i)
        maxDiff = juce::jmax (maxDiff, std::abs (buf[(size_t) i] - ref[(size_t) i]));
    check (maxDiff > 0.01f, "saturación altera la señal (maxDiff=" + juce::String (maxDiff, 4) + ")");

    // Con bias > 0, debe haber contenido en armónico par (FFT)
    // Simplificación: verifica que la media no es cero (DC de asimetría residual)
    double mean = 0.0;
    for (int i = N / 2; i < N; ++i)   // segunda mitad (transitorio inicial ya pasó)
        mean += buf[(size_t) i];
    mean /= (N / 2);
    check (std::abs (mean) > 1.0e-6f, "asimetría produce componente DC/par");
}

static void testSpectrumAnalyzer()
{
    std::cout << "[test] SpectrumAnalyzer FFT\n";

    SpectrumAnalyzer sa;
    sa.prepare (48000.0);

    // Empuja un seno de 1 kHz durante 4096 muestras (2 FFTs)
    const int N = SpectrumAnalyzer::fftSize * 2;
    std::vector<float> sig ((size_t) N);
    for (int i = 0; i < N; ++i)
        sig[(size_t) i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) i / 48000.0f);

    sa.push (sig.data(), N);

    std::vector<float> spec ((size_t) SpectrumAnalyzer::numBins, -200.0f);
    check (sa.getSpectrum (spec.data(), (int) spec.size()), "espectro disponible");

    // Encuentra el bin de mayor magnitud
    int maxBin = 0;
    float maxVal = -200.0f;
    for (int i = 1; i < SpectrumAnalyzer::numBins; ++i)   // saltar DC
    {
        if (spec[(size_t) i] > maxVal)
        {
            maxVal = spec[(size_t) i];
            maxBin = i;
        }
    }

    const double peakHz = sa.binToHz (maxBin);
    check (std::abs (peakHz - 1000.0) < 50.0,
           "pico FFT cerca de 1 kHz (" + juce::String (peakHz, 0) + " Hz)");
    check (maxVal > -70.0f, "magnitud del pico detectable (" + juce::String (maxVal, 1) + " dB)");
}

static void testProChannelValueTreeIntegration()
{
    std::cout << "[test] ProChannel: integración ValueTree ↔ motor\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);
    engine.attachTracks (tracks.getTracks());

    auto t = tracks.addTrack ("audio");
    check (t.isValid(), "pista creada");

    // El subtree proChannel existe con valores por defecto
    auto pc = t.getChildWithName (IDs::proChannel);
    check (pc.isValid(), "subtree proChannel creado");
    check (! (bool) pc.getProperty (IDs::pcEqEnabled), "EQ apagado por defecto");
    check ((float) pc.getProperty (IDs::pcEqLowFreq) == 80.0f, "low freq por defecto 80 Hz");

    const int slot = (int) t.getProperty (IDs::mixerSlot, -1);

    // Cambiar una propiedad del ProChannel debe propagarse al motor
    state.setProperty (pc, IDs::pcEqEnabled, true, "EQ on");
    state.setProperty (pc, IDs::pcEqMidGain, 6.0f, "Mid boost");

    auto& proc = engine.getProChannel (slot);
    check (proc.params.eqEnabled.load(), "eqEnabled propagado al ProChannelProcessor");
    check (std::abs (proc.params.eqMidGain.load() - 6.0f) < 0.01f, "eqMidGain propagado");
}

int main (int, char**)
{
    std::cout << "=== MiPropioDaw — tests de Fase 1 + 2 ===\n";

    testTrackCreationAndRouting();
    testUndoRedo();
    testSerializationRoundtrip();
    testSlotCapacity();
    testTransportText();

    // Fase 2
    testProChannelEq();
    testProChannelCompressor();
    testProChannelSaturation();
    testSpectrumAnalyzer();
    testProChannelValueTreeIntegration();

    std::cout << "\n" << (gChecks - gFailures) << "/" << gChecks << " comprobaciones correctas\n";

    if (gFailures > 0)
    {
        std::cout << gFailures << " FALLOS\n";
        return 1;
    }

    std::cout << "TODO OK\n";
    return 0;
}
