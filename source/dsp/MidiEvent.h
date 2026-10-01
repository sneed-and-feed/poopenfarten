#pragma once

#include <cstdint>

namespace ppf42 {

// ============================================================================
// MidiEvent: Lightweight POD MIDI representation decoupled from JUCE
// Enables headless test execution without audio framework dependencies.
// ============================================================================
struct MidiEvent {
    int sampleOffset { 0 };
    uint8_t status   { 0 };
    uint8_t data1    { 0 };
    uint8_t data2    { 0 };
};

} // namespace ppf42
