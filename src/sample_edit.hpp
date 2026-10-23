#pragma once
#include "model.hpp"
namespace vamp {
enum class SampleEdit{Trim,Delete,Silence,Reverse,Normalize,FadeIn,FadeOut,RemoveDC,Gain};
std::shared_ptr<Sample>editSample(const Sample&source,SampleEdit action,size_t first,size_t last,double value=1);
std::shared_ptr<Sample>extractSample(const Sample&source,size_t first,size_t last);
std::shared_ptr<Sample>pasteSample(const Sample&source,const Sample&clipboard,size_t frame,bool replace=false);
std::shared_ptr<Sample>stretchSample(const Sample&source,double durationRatio,const Sample*reference=nullptr);
std::shared_ptr<Sample>pitchSample(const Sample&source,double semitones);
std::vector<Slice>sliceSample(const Sample&source,int count,bool transients=false);
struct SampleAnalysis{double bpm=0,tempoConfidence=0,keyConfidence=0;std::string key;};
SampleAnalysis analyzeSample(const Sample&source);
std::shared_ptr<Sample>readAudio(const fs::path&path);
}
