#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <chrono>
#include "ProChannel.h"
#include "SpectrumAnalyzer.h"
#include "MidiSynth.h"
#include "MidiSequence.h"

// Motor de audio de baja latencia.
//  - El hilo de audio SOLO lee/writes atómicos (sin locks, sin allocations).
//  - El ValueTree (hilo de mensaje) se refleja en "slots" preasignados.
class AudioEngine : public juce::AudioIODeviceCallback,
                    private juce::ValueTree::Listener
{
public:
    static constexpr int kMaxSlots      = 128;   // = máx. de pistas por sesión
    static constexpr int kNoInput       = -1;    // sin fuente
    static constexpr int kTestToneInput = -2;    // tono de prueba 440 Hz

    // Estado por canal (hilo de audio lee; hilo de mensaje escribe)
    struct Slot
    {
        std::atomic<bool>  reserved { false };
        std::atomic<bool>  active   { false };
        std::atomic<bool>  isMidi   { false };
        std::atomic<int>   inputChannel { kNoInput };
        std::atomic<float> gain  { 1.0f };   // lineal
        std::atomic<float> pan   { 0.0f };   // -1..1
        std::atomic<bool>  mute  { false };
        std::atomic<bool>  solo  { false };
        std::atomic<float> peak  { 0.0f };
        std::atomic<float> rms   { 0.0f };
        float tonePhase = 0.0f;              // sólo hilo de audio

        ProChannelProcessor proChannel;      // DSP por canal (sólo hilo de audio procesa)
        MidiSynth synth;                     // sólo hilo de audio procesa
    };

    AudioEngine();
    ~AudioEngine() override;

    // --- dispositivos -------------------------------------------------
    bool initDevice (juce::String& errorMessage);
    juce::AudioDeviceManager& deviceManager() noexcept { return mgr; }
    int numDeviceInputs() const  { return deviceInputs.load (std::memory_order_relaxed); }

    // --- modelo -------------------------------------------------------
    void attachTracks   (const juce::ValueTree& tracksTree);
    void attachPatterns (const juce::ValueTree& patternsTree);
    void attachTransport(const juce::ValueTree& transportTree);
    void attachMixer    (const juce::ValueTree& mixerTree);

    int reserveSlot();   // hilo de mensaje; -1 si no quedan huecos
    const Slot& getSlot (int index) const { return slots[(size_t) juce::jlimit (0, kMaxSlots - 1, index)]; }

    // --- preview MIDI (hilo de mensaje → hilo de audio) ----------------
    void previewNoteOn  (int slot, int pitch, float velocity01 = 0.8f);
    void previewNoteOff (int slot, int pitch);

    // --- sesión (hilo de mensaje lanza/para; hilo de audio consume) -----
    void launchSessionClip (int slot, int patternUid);
    void stopSessionClip (int slot);
    void stopAllSessionClips();
    int  processSession (double beatStart, double beatEnd, int numSamples, bool discontinuity = false);

    bool  isSessionActive (int slot) const;
    int   sessionQueuedPatternFor (int slot) const;
    int   sessionPatternFor (int slot) const;
    float sessionLoopProgress (int slot) const;

    void  setSessionQuantize (float beats) { sessionQuantize.store (juce::jmax (0.0f, beats), std::memory_order_relaxed); }
    float getSessionQuantize() const       { return sessionQuantize.load (std::memory_order_relaxed); }

    // --- transporte ---------------------------------------------------
    void play()  { playing.store (true,  std::memory_order_relaxed); }
    void stop();
    void togglePlay() { isPlaying() ? stop() : play(); }
    bool isPlaying() const { return playing.load (std::memory_order_relaxed); }

    double positionBeats() const;
    juce::String positionText() const;
    void setPlayheadBeats (double beats);

    double getBpm() const          { return bpm.load (std::memory_order_relaxed); }
    int getTimeSigNum() const      { return timeSigNum.load (std::memory_order_relaxed); }
    bool isMetronomeOn() const     { return metronomeOn.load (std::memory_order_relaxed); }
    bool isRecordArmed() const     { return recordArmed.load (std::memory_order_relaxed); }

    // --- medición -----------------------------------------------------
    float getCpuPercent() const    { return cpu.load (std::memory_order_relaxed); }
    float getLatencyMs() const;
    double getSampleRate() const   { return sampleRate.load (std::memory_order_relaxed); }
    int getBlockSize() const       { return blockSizeSamples.load (std::memory_order_relaxed); }

    float masterPeakValue() const  { return masterPeak.load (std::memory_order_relaxed); }
    float masterRmsValue() const   { return masterRms.load (std::memory_order_relaxed); }

    // --- ProChannel / spectrum ---------------------------------------
    ProChannelProcessor& getProChannel (int slotIndex)
    {
        return slots[(size_t) juce::jlimit (0, kMaxSlots - 1, slotIndex)].proChannel;
    }

    SpectrumAnalyzer& masterSpectrum() noexcept { return spectrum; }

    // Actualiza los parámetros del ProChannel de un slot desde su ValueTree.
    void syncProChannelFromTree (const juce::ValueTree& trackTree);

    // --- AudioIODeviceCallback ---------------------------------------
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct SessionLoop
    {
        static constexpr int kMaxLoopEvents = 4096;

        std::array<SeqEvent, kMaxLoopEvents> events {};
        int count = 0;
        float loopBeats = 4.0f;
        std::atomic<uint32_t> generation { 0 };
        std::atomic<int>  pendingPattern { -1 };
        std::atomic<bool> pendingStop { false };
        std::atomic<bool> active { false };
        std::atomic<int>  activePattern { -1 };
        std::atomic<float> progress { 0.0f };
        double loopStartBeat = 0.0;
        std::array<uint8_t, 128> sounding {};
    };

    void activateFromTree (const juce::ValueTree& trackTree);
    void syncSlotFromTree (const juce::ValueTree& trackTree);
    void syncTransport();
    static int slotIndexFromTree (const juce::ValueTree& trackTree);

    void rebuildSequence (int slotIndex);
    void rebuildAllSequences();
    void rebuildLoop (int slotIndex, int patternUid);
    void rebuildActiveLoops();
    void collectEvents (int slotIndex, float beatStart, float beatEnd,
                        SeqEvent* out, int maxOut, int& outCount);
    void releaseLoopNotes (int slotIndex, SessionLoop& loop);
    void resetSessionLoops();

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    juce::AudioDeviceManager mgr;
    juce::ValueTree tracksTree, patternsTree, transportTree, mixerTree;   // hilo de mensaje

    std::array<Slot, kMaxSlots> slots;
    std::vector<float> scratch;   // preasignada en aboutToStart

    std::array<SlotSequence*, kMaxSlots> seqs {};   // sólo pistas MIDI
    std::array<SessionLoop*, kMaxSlots> sessionLoops {};
    std::vector<SeqEvent> seqScratch;              // sólo hilo de mensaje
    SynthCommandQueue<256> synthQueue;
    std::atomic<bool> transportResetFlag { false };
    std::atomic<float> sessionQuantize { 4.0f };

    SpectrumAnalyzer spectrum;    // FFT del master (hilo audio empuja, UI lee)

    // transporte (accesible desde ambos hilos)
    std::atomic<bool>   playing        { false };
    std::atomic<bool>   metronomeOn    { false };
    std::atomic<bool>   recordArmed    { false };
    std::atomic<int64_t> playheadSamples { 0 };
    std::atomic<double> bpm            { 140.0 };
    std::atomic<double> sampleRate     { 48000.0 };
    std::atomic<int>    timeSigNum     { 4 };
    std::atomic<int>    blockSizeSamples { 256 };
    std::atomic<int>    deviceInputs   { 0 };
    std::atomic<float>  cpu            { 0.0f };
    std::atomic<float>  masterGainLin  { 1.0f };
    std::atomic<float>  masterPeak     { 0.0f };
    std::atomic<float>  masterRms      { 0.0f };

    // sólo hilo de audio (lastBeat puede ser tocado desde setPlayheadBeats)
    float clickPhase = 0.0f;
    float clickEnv   = 0.0f;
    double clickFreq = 1100.0;
    double prevEndBeat = -1.0;
    std::atomic<int> lastBeat { std::numeric_limits<int>::min() };

    bool deviceInitialised = false;
    juce::String lastDeviceError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
