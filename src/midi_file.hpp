#pragma once
#include "model.hpp"
namespace vamp {
struct MidiImport {std::vector<TempoPoint>tempoMap;std::vector<MeterPoint>meterMap;std::vector<Track> tracks;double bpm=120;int bars=1;bool tempoChanges=false,otherMessages=false,splitNotes=false;};
MidiImport readMidi(const fs::path&path);
void writeMidi(const Project&project,const fs::path&path);
}
