#pragma once

namespace IDs
{
// raíz
inline constexpr const char* project     = "project";
inline constexpr const char* version     = "version";

// transporte
inline constexpr const char* transport   = "transport";
inline constexpr const char* bpm         = "bpm";
inline constexpr const char* timeSigNum  = "timeSigNum";
inline constexpr const char* timeSigDen  = "timeSigDen";
inline constexpr const char* metronome   = "metronome";
inline constexpr const char* loop        = "loop";
inline constexpr const char* record      = "record";

// pistas
inline constexpr const char* tracks      = "tracks";
inline constexpr const char* track       = "track";
inline constexpr const char* type        = "type";
inline constexpr const char* name        = "name";
inline constexpr const char* colour      = "colour";
inline constexpr const char* mixerSlot   = "mixerSlot";
inline constexpr const char* inputChannel = "inputChannel";
inline constexpr const char* gain        = "gain";   // dB
inline constexpr const char* pan         = "pan";    // -1..1
inline constexpr const char* mute        = "mute";
inline constexpr const char* solo        = "solo";

// patrones
inline constexpr const char* patterns    = "patterns";
inline constexpr const char* pattern     = "pattern";
inline constexpr const char* patternLen  = "patternLen"; // beats
inline constexpr const char* activePattern = "activePattern"; // uid del patrón en edición

// notas (hijas de pattern)
inline constexpr const char* note        = "note";
inline constexpr const char* pitch       = "pitch";     // 0..127
inline constexpr const char* start       = "start";     // beats
inline constexpr const char* duration    = "duration";  // beats
inline constexpr const char* velocity    = "velocity";  // 0..1

// clips de arreglo (hijos de track)
inline constexpr const char* clip        = "clip";
inline constexpr const char* uid         = "uid";
inline constexpr const char* patternUid  = "patternUid";

inline constexpr const char* viewMode    = "viewMode";

inline constexpr const char* session     = "session";
inline constexpr const char* scene       = "scene";
inline constexpr const char* sessionClip = "sessionClip";
inline constexpr const char* sceneUid    = "sceneUid";
inline constexpr const char* quantize    = "quantize";

// estado de vista del editor MIDI
inline constexpr const char* scaleRoot   = "scaleRoot"; // 0..11 (C..B)
inline constexpr const char* scaleId     = "scaleId";   // nombre de escala
inline constexpr const char* snapDiv     = "snapDiv";   // pulsos por unidad de rejilla

// mixer
inline constexpr const char* mixer       = "mixer";
inline constexpr const char* masterGain  = "masterGain"; // dB

// ProChannel (subtree bajo cada track)
inline constexpr const char* proChannel  = "proChannel";

// EQ
inline constexpr const char* pcEqEnabled  = "pcEqEnabled";
inline constexpr const char* pcEqLowGain  = "pcEqLowGain";   // dB
inline constexpr const char* pcEqLowFreq  = "pcEqLowFreq";   // Hz
inline constexpr const char* pcEqMidGain  = "pcEqMidGain";   // dB
inline constexpr const char* pcEqMidFreq  = "pcEqMidFreq";   // Hz
inline constexpr const char* pcEqMidQ     = "pcEqMidQ";
inline constexpr const char* pcEqHighGain = "pcEqHighGain";  // dB
inline constexpr const char* pcEqHighFreq = "pcEqHighFreq";  // Hz

// Compressor
inline constexpr const char* pcCompEnabled   = "pcCompEnabled";
inline constexpr const char* pcCompThreshold = "pcCompThreshold"; // dB
inline constexpr const char* pcCompRatio     = "pcCompRatio";
inline constexpr const char* pcCompAttack    = "pcCompAttack";    // ms
inline constexpr const char* pcCompRelease   = "pcCompRelease";   // ms
inline constexpr const char* pcCompMakeup    = "pcCompMakeup";    // dB

// Saturación
inline constexpr const char* pcSatEnabled = "pcSatEnabled";
inline constexpr const char* pcSatDrive   = "pcSatDrive";   // 0..1
inline constexpr const char* pcSatBias    = "pcSatBias";    // -0.5..0.5
inline constexpr const char* pcSatOutput  = "pcSatOutput";  // dB
}
