#pragma once
#include "model.hpp"
#include <set>
namespace vamp {
enum class NoteTool{Quantize,Humanize,RandomVelocity,Legato,Chop,Glue,FlipTime,FlipPitch,Strum,Arpeggiate,FitScale,Transpose};
struct NoteToolOptions{double grid=.25,swing=0,strength=1,amount=.15;int root=0,scale=0,transpose=12;uint32_t seed=1;};
std::vector<Note>processNotes(const std::vector<Note>&source,const std::set<int>&selection,NoteTool tool,const NoteToolOptions&options,double patternLength);
std::vector<int>chordIntervals(int chord);
bool scaleContains(int pitch,int root,int scale);
}

namespace vamp {
constexpr int ScaleCount=14;
inline const wchar_t*scaleLabel(int index){const wchar_t*names[]={L"Мажор / ионийский",L"Натуральный минор / эолийский",L"Гармонический минор",L"Дорийский",L"Лидийский",L"Фригийский",L"Миксолидийский",L"Локрийский",L"Мелодический минор",L"Гармонический мажор",L"Мажорная пентатоника",L"Минорная пентатоника",L"Блюзовый",L"Хроматический"};return names[(int)clamp(index,0,ScaleCount-1)];}
}
