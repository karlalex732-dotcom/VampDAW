#pragma once
#include <algorithm>
#include <cstdint>

namespace vamp::ui {
// UI-thread envelope: finite transitions stop requesting frames once settled.
struct HoverEnvelope {
    float value=0,from=0,target=0;
    uint64_t started=0;
    bool moving=false;
    void reset(){value=from=target=0;started=0;moving=false;}
    void advance(uint64_t now){
        if(!moving)return;
        float t=std::min(1.f,float(now>=started?now-started:0)/(target>from?160.f:260.f));
        value=from+(target-from)*t*t*(3-2*t);
        if(t>=1){value=target;moving=false;}
    }
    void aim(bool hovered,uint64_t now){advance(now);float next=hovered?1.f:0.f;if(next==target)return;from=value;target=next;started=now;moving=from!=target;}
};
}
