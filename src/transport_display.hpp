#pragma once
#include <array>
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdint>
namespace vamp {
inline std::array<std::wstring,3>transportDigits(double beat){if(!std::isfinite(beat)||beat<0)beat=0;beat=std::min(beat,16384.);int64_t ticks=(int64_t)std::floor(beat*96+1e-7);int64_t bar=ticks/(96*4)+1,within=ticks%(96*4);auto tick=std::to_wstring(within%96);if(tick.size()<2)tick=L"0"+tick;return {std::to_wstring(bar),std::to_wstring(within/96+1),tick};}
}
