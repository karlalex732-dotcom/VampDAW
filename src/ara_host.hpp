#pragma once
#include "model.hpp"
#include "ara/ARAVST3.h"
namespace vamp {
#if defined(VAMP_ENABLE_ARA) && VAMP_ENABLE_ARA
inline constexpr bool AraEnabled=true;
#else
inline constexpr bool AraEnabled=false;
#endif
struct AraClipInput{std::string id,name,cloneFrom;std::shared_ptr<Sample>audio;double start=0,duration=0,offset=0,gain=1,fadeIn=0,fadeOut=0;};
struct AraTrackContext{std::string document,track;double bpm=120;std::vector<TempoPoint>tempos;std::vector<MeterPoint>meters;std::vector<AraClipInput>clips;uint64_t signature=0;};
AraTrackContext makeAraTrackContext(Project&,int track);
struct AraTransportRequest{int playing=-1,cycle=-1;double position=-1,cycleStart=0,cycleDuration=0;bool range=false;};
class AraHost{
public:
    struct Impl;std::unique_ptr<Impl>impl;
    AraHost(Steinberg::FUnknown*,const AraTrackContext&);~AraHost();
    void close();bool needsUpdate(const AraTrackContext&)const;void sync(const AraTrackContext&);
    void poll();bool takeChanges();double progress()const;AraTransportRequest takeTransport();
    void select(const std::string&);Json state();void restore(const Json&);std::string name()const;const TempoMap&clock()const;
};
}
