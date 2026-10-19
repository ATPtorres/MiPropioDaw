#include <JuceHeader.h>
#include <iostream>

#include "../src/core/ProjectState.h"
#include "../src/model/TrackList.h"
#include "../src/model/PatternLibrary.h"
#include "../src/audio/AudioEngine.h"
#include "../src/audio/ProChannel.h"
#include "../src/audio/SpectrumAnalyzer.h"
#include "../src/audio/MidiSynth.h"
#include "../src/audio/MidiSequence.h"
#include "../src/ui/widgets/Scales.h"
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

// ---------------------------------------------------------------------------
// Tests de MIDI — Fase 3
// ---------------------------------------------------------------------------
static void testPatternNotes()
{
    std::cout << "[test] patrones y notas\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);
    engine.attachTracks (tracks.getTracks());
    engine.attachPatterns (state.patterns());

    PatternLibrary lib (state);

    auto pattern = lib.addPattern ("Lead", 2);
    check (pattern.isValid(), "patrón creado");
    check (lib.patternCount() == 1, "conteo de patrones == 1");
    check (lib.uidOf (pattern) > 0, "uid asignado");
    check (lib.activePatternUid() == lib.uidOf (pattern), "patrón activo tras crear");
    check (lib.patternLengthBeats (pattern) == 8.0, "longitud 2 compases = 8 pulsos");

    auto n1 = lib.addNote (pattern, 60, 0.0, 1.0, 0.8);
    lib.addNote (pattern, 64, 1.0, 1.0, 0.6);
    lib.addNote (pattern, 67, 2.0, 2.0, 1.0);

    check (lib.noteCount (pattern) == 3, "3 notas creadas");
    check ((int) n1.getProperty (IDs::pitch) == 60, "pitch guardado");
    check ((double) n1.getProperty (IDs::start) == 0.0, "posición guardada");

    auto hit = lib.noteAt (pattern, 64, 1.5);
    check (hit.isValid() && (int) hit.getProperty (IDs::pitch) == 64, "hit test encuentra la nota");

    state.setProperty (n1, IDs::pitch, 72, "Move note");
    check ((int) pattern.getChild (0).getProperty (IDs::pitch) == 72, "mueve nota");
    check (lib.noteAt (pattern, 60, 0.0).isValid() == false, "hit test actualizado");

    state.performUndo();
    check ((int) pattern.getChild (0).getProperty (IDs::pitch) == 60, "deshacer mueve nota");

    lib.deleteNote (pattern, n1);
    check (lib.noteCount (pattern) == 2, "borra nota");
    state.performUndo();
    check (lib.noteCount (pattern) == 3, "deshacer borra nota");
}

static void testMakeUnique()
{
    std::cout << "[test] Make Unique\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);
    engine.attachTracks (tracks.getTracks());
    engine.attachPatterns (state.patterns());

    PatternLibrary lib (state);

    auto pattern = lib.addPattern ("Bass", 1);
    lib.addNote (pattern, 36, 0.0, 1.0, 0.9);
    lib.addNote (pattern, 43, 2.0, 1.0, 0.9);

    auto midiTrack = tracks.addTrack ("midi");
    auto clip = lib.addClip (midiTrack, lib.uidOf (pattern), 0.0, 4.0);
    check (clip.isValid(), "clip creado");
    check (lib.clipCount() == 1, "conteo de clips == 1");

    check (lib.patternInUse (lib.uidOf (pattern)), "patrón en uso detectado");
    check (! lib.deletePattern (pattern), "borrar patrón en uso bloqueado");

    check (lib.makeUnique (clip), "make unique ejecuta");

    const int newUid = (int) clip.getProperty (IDs::patternUid, -1);
    check (newUid != lib.uidOf (pattern), "clip apunta a patrón nuevo");
    check (lib.patternCount() == 2, "patrón duplicado existe");

    auto newPattern = lib.patternForUid (newUid);
    check (lib.noteCount (newPattern) == lib.noteCount (pattern), "notas copiadas");

    lib.addNote (newPattern, 48, 3.0, 0.5, 0.5);
    check (lib.noteCount (newPattern) == 3, "añade nota al patrón unique");
    check (lib.noteCount (pattern) == 2, "el patrón original no cambia");

    check (lib.deletePattern (pattern), "original ya no en uso se puede borrar");
}

static void testClipSerialization()
{
    std::cout << "[test] serialización de patrones, notas y clips\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);
    engine.attachTracks (tracks.getTracks());
    engine.attachPatterns (state.patterns());

    PatternLibrary lib (state);

    auto pattern = lib.addPattern ("Drums", 1);
    lib.addNote (pattern, 36, 0.0, 0.25, 1.0);
    lib.addNote (pattern, 38, 1.0, 0.25, 0.7);

    auto midiTrack = tracks.addTrack ("midi");
    lib.addClip (midiTrack, lib.uidOf (pattern), 4.0, 4.0);

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("miproiodaw_test_f3.dawproj");

    check (state.save (file), "proyecto guardado");

    ProjectState loaded;
    check (loaded.load (file), "proyecto cargado");

    PatternLibrary lib2 (loaded);
    check (lib2.patternCount() == 1, "1 patrón restaurado");
    check (lib2.clipCount() == 1, "1 clip restaurado");

    auto p2 = lib2.patterns().getChild (0);
    check (lib2.noteCount (p2) == 2, "2 notas restauradas");
    check ((int) p2.getChild (0).getProperty (IDs::pitch) == 36, "pitch restaurado");
    check ((double) p2.getChild (1).getProperty (IDs::start) == 1.0, "posición restaurada");

    AudioEngine engine2;
    engine2.attachTracks (loaded.tracks());
    engine2.attachPatterns (loaded.patterns());

    const int slot = (int) loaded.tracks().getChild (loaded.tracks().getNumChildren() - 1)
                          .getProperty (IDs::mixerSlot, -1);
    check (engine2.getSlot (slot).isMidi.load(), "slot marcado como MIDI");

    file.deleteFile();
}

static void testMidiSequence()
{
    std::cout << "[test] secuencia MIDI del motor\n";

    ProjectState state;
    AudioEngine engine;
    TrackList tracks (state, engine);
    PatternLibrary lib (state);

    auto pattern = lib.addPattern ("Seq", 1);
    lib.addNote (pattern, 60, 0.0, 1.0, 1.0);
    lib.addNote (pattern, 62, 1.0, 1.0, 1.0);

    auto midiTrack = tracks.addTrack ("midi");
    lib.addClip (midiTrack, lib.uidOf (pattern), 0.0, 4.0);

    const int slot = (int) midiTrack.getProperty (IDs::mixerSlot, -1);

    SeqEvent events[256];
    const int n = MidiSequence::build (tracks.getTracks(), state.patterns(), slot, events, 256);

    check (n == 4, "4 eventos (2 on + 2 off) (" + juce::String (n) + ")");
    check (events[0].type == 0 && events[0].pitch == 60 && events[0].beat == 0.0f, "primer on en 0.0");
    check (events[1].type == 1 && events[1].beat == 1.0f, "primer off en 1.0");
    check (events[2].type == 0 && events[2].pitch == 62 && events[2].beat == 1.0f, "segundo on en 1.0");

    // recorte al final del clip: nota que se extiende más allá del clip
    lib.addNote (pattern, 64, 3.5, 2.0, 1.0);
    const int n2 = MidiSequence::build (tracks.getTracks(), state.patterns(), slot, events, 256);
    check (n2 == 6, "6 eventos tras añadir nota larga");
    check (events[5].beat <= 4.0f, "off recortado al final del clip");

    // clip de otra pista con otro patrón no contamina
    auto other = lib.addPattern ("Other", 1);
    lib.addNote (other, 72, 0.0, 1.0, 1.0);
    auto midi2 = tracks.addTrack ("midi");
    lib.addClip (midi2, lib.uidOf (other), 0.0, 4.0);

    const int slot2 = (int) midi2.getProperty (IDs::mixerSlot, -1);
    const int n3 = MidiSequence::build (tracks.getTracks(), state.patterns(), slot2, events, 256);
    check (n3 == 2, "secuencia aislada por slot (" + juce::String (n3) + ")");
}

static void testMidiSynth()
{
    std::cout << "[test] sintetizador MIDI\n";

    MidiSynth synth;
    synth.prepare (48000.0);

    std::vector<float> buf (512, 0.0f);
    synth.noteOn (60, 1.0f);
    synth.render (buf.data(), 512);

    float maxAbs = 0.0f;
    for (float v : buf)
        maxAbs = std::max (maxAbs, std::abs (v));
    check (maxAbs > 0.01f, "nota suena (max=" + juce::String (maxAbs, 4) + ")");

    std::fill (buf.begin(), buf.end(), 0.0f);
    synth.noteOff (60);
    for (int i = 0; i < 40; ++i)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        synth.render (buf.data(), 512);
    }

    float maxTail = 0.0f;
    for (float v : buf)
        maxTail = std::max (maxTail, std::abs (v));
    check (maxTail < maxAbs * 0.5f, "release decae (" + juce::String (maxTail, 4) + ")");

    std::fill (buf.begin(), buf.end(), 0.0f);
    synth.allOff();
    synth.render (buf.data(), 512);
    maxTail = 0.0f;
    for (float v : buf)
        maxTail = std::max (maxTail, std::abs (v));
    check (maxTail < 1.0e-6f, "allOff silencia");

    // cola de comandos
    SynthCommandQueue<16> q;
    check (q.push ({ 1, 60, 100, 0 }), "push ok");
    SynthCommand c;
    check (q.pop (c) && c.pitch == 60 && c.type == 0, "pop ok");
    check (! q.pop (c), "cola vacía");
}

static void testScales()
{
    std::cout << "[test] escalas (scale highlighting)\n";

    check (Scales::intervalsFor ("Major").size() == 7, "mayor tiene 7 grados");
    check (Scales::inScale (60, 0, Scales::intervalsFor ("Major")), "C en Do mayor");
    check (! Scales::inScale (61, 0, Scales::intervalsFor ("Major")), "C# fuera de Do mayor");
    check (Scales::inScale (62, 0, Scales::intervalsFor ("Major")), "D en Do mayor");
    check (Scales::inScale (0, 0, Scales::intervalsFor ("Off")), "escala Off = todas");
    check (Scales::noteName (60) == "C4", "nombre de nota C4");
    check (Scales::noteName (69) == "A4", "nombre de nota A4");
}

int main (int, char**)
{
    std::cout << "=== MiPropioDaw — tests de Fase 1 + 2 + 3 ===\n";

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

    // Fase 3
    testPatternNotes();
    testMakeUnique();
    testClipSerialization();
    testMidiSequence();
    testMidiSynth();
    testScales();

    std::cout << "\n" << (gChecks - gFailures) << "/" << gChecks << " comprobaciones correctas\n";

    if (gFailures > 0)
    {
        std::cout << gFailures << " FALLOS\n";
        return 1;
    }

    std::cout << "TODO OK\n";
    return 0;
}
