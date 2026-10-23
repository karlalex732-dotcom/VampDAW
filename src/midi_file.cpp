#include "midi_file.hpp"
#include "engine.hpp"
#include <deque>
#include <map>
#include <stdexcept>

namespace vamp {
namespace {
struct Reader {
    std::vector<uint8_t> bytes;size_t pos=0,end=0;
    uint8_t byte(){if(pos>=end)throw std::runtime_error("Truncated MIDI file");return bytes[pos++];}
    uint32_t be(int n){uint32_t v=0;while(n--)v=(v<<8)|byte();return v;}
    uint32_t vlq(){uint32_t v=0;for(int i=0;i<4;i++){auto b=byte();v=(v<<7)|(b&127);if(!(b&128))return v;}throw std::runtime_error("Invalid MIDI variable length value");}
    void skip(size_t n){if(n>end-pos)throw std::runtime_error("Invalid MIDI chunk length");pos+=n;}
};
void be(std::vector<uint8_t>&b,uint32_t v,int n){for(int i=n-1;i>=0;i--)b.push_back(uint8_t(v>>(i*8)));}
void vlq(std::vector<uint8_t>&b,uint32_t v){if(v>0x0fffffff)throw std::runtime_error("MIDI event delta too long");uint8_t a[4];int n=0;a[n++]=v&127;while(v>>=7)a[n++]=(v&127)|128;while(n)b.push_back(a[--n]);}
void chunk(std::ofstream&f,const char*tag,const std::vector<uint8_t>&b){f.write(tag,4);std::vector<uint8_t>n;be(n,(uint32_t)b.size(),4);f.write((const char*)n.data(),4);f.write((const char*)b.data(),b.size());}
}
MidiImport readMidi(const fs::path&path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f||f.tellg()<14||f.tellg()>32*1024*1024)throw std::runtime_error("MIDI file must be 14 bytes to 32 MB");
    Reader r;r.bytes.resize((size_t)f.tellg());f.seekg(0);f.read((char*)r.bytes.data(),r.bytes.size());r.end=r.bytes.size();
    if(r.be(4)!=0x4d546864)throw std::runtime_error("Not a Standard MIDI File");auto size=r.be(4);if(size<6||size>r.end-r.pos)throw std::runtime_error("Invalid MIDI header");int format=r.be(2),count=r.be(2),ppqn=r.be(2);r.skip(size-6);
    if(format>1||count<1||count>1024||(format==0&&count!=1)||!ppqn||(ppqn&0x8000))throw std::runtime_error("Supported MIDI: format 0/1, PPQN timing (not SMPTE)");
    MidiImport result;uint64_t tempoTick=UINT64_MAX;uint32_t firstTempo=500000;size_t totalNotes=0,totalEvents=0,expandedNotes=0;double songEnd=0;
    for(int ti=0;ti<count;){if(r.end-r.pos<8)throw std::runtime_error("Missing MIDI track");auto tag=r.be(4),len=r.be(4);if(len>r.end-r.pos)throw std::runtime_error("Invalid MIDI track length");if(tag!=0x4d54726b){r.skip(len);continue;}ti++;size_t fileEnd=r.end;r.end=r.pos+len;uint64_t tick=0;uint8_t running=0;std::string name="MIDI "+std::to_string(ti);
        struct Held{uint64_t tick;float velocity;};std::array<std::deque<Held>,2048> held;std::array<bool,16>sustain{};std::array<std::vector<std::pair<int,Held>>,16>released;std::map<int,std::vector<Note>>channels;
        auto finish=[&](int ch,int pitch,Held n){if(++totalNotes>1000000)throw std::runtime_error("MIDI import exceeds one million notes");double start=n.tick/double(ppqn),end=tick/double(ppqn);if(end>16384)throw std::runtime_error("MIDI exceeds 4096 bars");channels[ch].push_back({start,std::max(1./ppqn,end-start),pitch,n.velocity});songEnd=std::max(songEnd,end);};
        bool ended=false;while(r.pos<r.end&&!ended){if(++totalEvents>4000000)throw std::runtime_error("MIDI event limit exceeded");tick+=r.vlq();if(tick>uint64_t(ppqn)*16384)throw std::runtime_error("MIDI exceeds 4096 bars");auto status=r.byte();if(status<128){if(!running)throw std::runtime_error("MIDI running status without status");r.pos--;status=running;}
            if(status==0xff){running=0;auto type=r.byte();auto n=r.vlq();if(n>r.end-r.pos)throw std::runtime_error("Truncated MIDI meta event");if(type==3){name.assign((const char*)r.bytes.data()+r.pos,std::min<uint32_t>(n,256));for(auto&c:name)if((unsigned char)c<32)c=' ';}
                if(type==0x51&&n==3){uint32_t tempo=(r.bytes[r.pos]<<16)|(r.bytes[r.pos+1]<<8)|r.bytes[r.pos+2];if(!tempo)throw std::runtime_error("Invalid MIDI tempo");result.tempoMap.push_back({tick/double(ppqn),60000000./tempo});if(tick<tempoTick){if(tempoTick!=UINT64_MAX&&tempo!=firstTempo)result.tempoChanges=true;tempoTick=tick;firstTempo=tempo;}else if(tempo!=firstTempo)result.tempoChanges=true;}
                if(type==0x58&&n>=2){int den=r.bytes[r.pos+1];if(den>5)throw std::runtime_error("MIDI time signature denominator exceeds 32");result.meterMap.push_back({tick/double(ppqn),(int)r.bytes[r.pos],1<<den});}if(type==0x2f){if(n)throw std::runtime_error("Invalid MIDI end marker");ended=true;}r.skip(n);
            }else if(status==0xf0||status==0xf7){running=0;r.skip(r.vlq());result.otherMessages=true;}
            else if(status>=0x80&&status<0xf0){running=status;int kind=status&0xf0,ch=status&15,a=r.byte(),b=(kind==0xc0||kind==0xd0)?0:r.byte();if(a>127||b>127)throw std::runtime_error("Invalid MIDI data byte");auto&q=held[ch*128+a];if(kind==0x90&&b)q.push_back({tick,b/127.f});else if(kind==0x80||kind==0x90){if(!q.empty()){auto n=q.front();q.pop_front();if(sustain[ch])released[ch].push_back({a,n});else finish(ch,a,n);}}
                else if(kind==0xb0&&a==64){bool down=b>=64;if(sustain[ch]&&!down){for(auto&n:released[ch])finish(ch,n.first,n.second);released[ch].clear();}sustain[ch]=down;}
                else result.otherMessages=true;
            }else throw std::runtime_error("Invalid status in MIDI file");
        }
        if(!ended)throw std::runtime_error("MIDI track is missing its end marker");
        for(int ch=0;ch<16;ch++){for(int pitch=0;pitch<128;pitch++)for(auto&n:held[ch*128+pitch])finish(ch,pitch,n);for(auto&n:released[ch])finish(ch,n.first,n.second);}
        r.pos=r.end;r.end=fileEnd;
        for(auto&channel:channels){if(result.tracks.size()>=TrackCount)throw std::runtime_error("MIDI has more than 512 note tracks/channels");Track t;t.name=name+" / ch "+std::to_string(channel.first+1);t.midiChannel=channel.first+1;t.color=(int)result.tracks.size()%5;t.patternLength=64;std::map<int,std::vector<Note>>blocks;
            for(auto n:channel.second){double end=n.beat+n.length;while(n.beat<end-1e-9){int block=(int)(n.beat/64);double stop=std::min(end,(block+1)*64.);if(++expandedNotes>1000000)throw std::runtime_error("Expanded MIDI note limit exceeded");blocks[block].push_back({n.beat-block*64,stop-n.beat,n.pitch,n.velocity});if(stop<end-1e-9)result.splitNotes=true;n.beat=stop;}}
            for(auto&block:blocks){if(block.second.size()>100000)throw std::runtime_error("MIDI pattern exceeds 100000 notes");std::sort(block.second.begin(),block.second.end(),[](auto&a,auto&b){return a.beat<b.beat;});int id=0;if(t.clips.empty()){t.notes=std::move(block.second);t.patternName="MIDI / "+std::to_string(block.first+1);}else{if(t.patterns.size()>=255)throw std::runtime_error("MIDI exceeds 256 patterns");t.patterns.push_back({"MIDI / "+std::to_string(block.first+1),64,std::move(block.second)});id=(int)t.patterns.size();}double length=std::min(64.,std::max(1.,std::ceil(songEnd/4)*4-block.first*64));t.clips.push_back({block.first*64.,length,id});}result.tracks.push_back(std::move(t));}
    }
    if(result.tracks.empty())throw std::runtime_error("MIDI file contains no notes");auto unique=[](auto&points){std::stable_sort(points.begin(),points.end(),[](auto&a,auto&b){return a.beat<b.beat;});for(size_t i=1;i<points.size();)if(points[i].beat==points[i-1].beat)points.erase(points.begin()+i-1);else i++;};unique(result.tempoMap);unique(result.meterMap);for(auto&t:result.tempoMap){if(t.bpm<MinTempo-.000001||t.bpm>MaxTempo+.01)throw std::runtime_error("MIDI tempo outside supported range 10..999 BPM");t.bpm=clamp(t.bpm,MinTempo,MaxTempo);}result.bpm=!result.tempoMap.empty()&&result.tempoMap.front().beat==0?result.tempoMap.front().bpm:120;TempoMap(result.bpm,result.tempoMap,result.meterMap);if(result.bpm<MinTempo-.000001||result.bpm>MaxTempo+.01)throw std::runtime_error("MIDI tempo outside supported range 10..999 BPM");result.bpm=clamp(result.bpm,MinTempo,MaxTempo);if(tempoTick!=UINT64_MAX&&tempoTick>0)result.tempoChanges=true;result.bars=barsForEnd(songEnd);return result;
}
void writeMidi(const Project&p,const fs::path&path){
    auto plan=compile(p);struct E{uint32_t tick;int pitch,velocity;bool on;};std::vector<std::vector<E>>tracks(p.tracks.size());
    for(auto&e:plan->events)if(!p.tracks[e.track].sample||p.tracks[e.track].sampler){auto tick=(uint32_t)std::llround(e.beat*960);tracks[e.track].push_back({tick,e.pitch,(int)clamp(std::round(e.velocity*127),1,127),e.on});}
    int count=1;for(auto&t:tracks)if(!t.empty())count++;auto temp=path;temp+=L".tmp";std::ofstream f(temp,std::ios::binary);if(!f)throw std::runtime_error("Cannot write MIDI");try{std::vector<uint8_t>header;be(header,1,2);be(header,count,2);be(header,960,2);chunk(f,"MThd",header);struct Meta{uint32_t tick;std::vector<uint8_t>data;};std::vector<Meta>changes;auto clock=songClock(p);auto addTempo=[&](double beat,double bpm){std::vector<uint8_t>data{0xff,0x51,3};be(data,(uint32_t)std::llround(60000000./bpm),3);changes.push_back({(uint32_t)std::llround(beat*960),std::move(data)});};auto addMeter=[&](double beat,int n,int den){int power=0;while((1<<power)<den)power++;changes.push_back({(uint32_t)std::llround(beat*960),{0xff,0x58,4,(uint8_t)n,(uint8_t)power,24,8}});};addTempo(0,clock.bpmAt(0));for(auto&t:p.tempoMap)if(t.beat>0)addTempo(t.beat,t.bpm);auto meter=clock.meterAt(0);addMeter(0,meter.numerator,meter.denominator);for(auto&m:p.meterMap)if(m.beat>0)addMeter(m.beat,m.numerator,m.denominator);std::stable_sort(changes.begin(),changes.end(),[](auto&a,auto&b){return a.tick<b.tick;});std::vector<uint8_t>meta;uint32_t prior=0;for(auto&m:changes){vlq(meta,m.tick-prior);meta.insert(meta.end(),m.data.begin(),m.data.end());prior=m.tick;}meta.insert(meta.end(),{0,0xff,0x2f,0});chunk(f,"MTrk",meta);
        for(size_t i=0;i<tracks.size();i++)if(!tracks[i].empty()){auto&events=tracks[i];std::stable_sort(events.begin(),events.end(),[](auto&a,auto&b){if(a.tick!=b.tick)return a.tick<b.tick;return a.on<b.on;});std::vector<uint8_t>b{0,0xff,3};auto name=p.tracks[i].name.substr(0,256);vlq(b,(uint32_t)name.size());b.insert(b.end(),name.begin(),name.end());uint32_t previous=0;for(auto&e:events){vlq(b,e.tick-previous);previous=e.tick;b.push_back((e.on?0x90:0x80)|(p.tracks[i].midiChannel-1));b.push_back(e.pitch);b.push_back(e.on?e.velocity:0);}b.insert(b.end(),{0,0xff,0x2f,0});chunk(f,"MTrk",b);}
        f.flush();if(!f)throw std::runtime_error("MIDI disk write failed");f.close();if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace MIDI file");
    }catch(...){f.close();std::error_code ec;fs::remove(temp,ec);throw;}
}
}
