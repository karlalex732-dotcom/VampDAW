#pragma once
#include "model.hpp"
namespace vamp {
inline int sortPlaylistMarkers(Project&p,int selected){
    if(selected<0||selected>=(int)p.markers.size())return -1;
    double beat=p.markers[selected].beat;int next=0;for(int i=0;i<(int)p.markers.size();i++)if(p.markers[i].beat<beat||(p.markers[i].beat==beat&&i<selected))next++;
    std::stable_sort(p.markers.begin(),p.markers.end(),[](auto&a,auto&b){return a.beat<b.beat;});return next;
}
inline bool validPlaylistMarker(const Project&p,const Marker&m){return std::isfinite(m.beat)&&m.beat>=0&&m.beat<=p.bars*4.&&!m.name.empty()&&m.name.size()<=1024&&wide(m.name).size()<=256&&m.color>=0&&m.color<5;}
inline int updatePlaylistMarker(Project&p,int index,const Marker&m){
    if(index<0||index>=(int)p.markers.size()||!validPlaylistMarker(p,m))return -1;
    p.markers[index]=m;return sortPlaylistMarkers(p,index);
}
inline bool deletePlaylistMarker(Project&p,int index){if(index<0||index>=(int)p.markers.size())return false;p.markers.erase(p.markers.begin()+index);return true;}
}
