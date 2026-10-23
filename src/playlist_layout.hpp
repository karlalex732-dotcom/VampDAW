#pragma once
#include <algorithm>
#include <vector>
#include <cmath>

namespace vamp {
// The same variable-height geometry is used for painting, drops and selection.
struct PlaylistLayout {
    std::vector<float> edges{0};float viewport=1;
    void reset(const std::vector<float>&heights,float height){viewport=std::max(1.f,height);edges.assign(1,0);for(float h:heights)edges.push_back(edges.back()+std::min(viewport,std::max(24.f,h)));}
    int count()const{return (int)edges.size()-1;}
    int maxOffset()const{if(count()==0||edges.back()<=viewport)return 0;return std::min(count()-1,(int)(std::lower_bound(edges.begin(),edges.end(),edges.back()-viewport)-edges.begin()));}
    float height(int row)const{return row>=0&&row<count()?edges[row+1]-edges[row]:24;}
    float y(double row,int offset)const{int i=(int)std::floor(row);if(i<0)return (float)row*height(0)-edges[std::min(offset,count())];if(i>=count())return edges.back()+(float)(row-count())*height(count()-1)-edges[std::min(offset,count())];return edges[i]+(float)(row-i)*height(i)-edges[std::min(offset,count())];}
    double row(float y,int offset)const{if(count()==0)return 0;float at=std::max(0.f,y)+edges[std::min(offset,count())];int i=(int)(std::upper_bound(edges.begin(),edges.end(),at)-edges.begin())-1;i=std::clamp(i,0,count()-1);return i+std::clamp((at-edges[i])/height(i),0.f,.99999f);}
    int end(int offset)const{return count()?std::min(count(),1+(int)std::floor(row(viewport-.001f,offset))):0;}
    int hit(float y,int offset)const{return y<0||y>=viewport||count()==0||y+edges[std::min(offset,count())]>=edges.back()?-1:(int)std::floor(row(y,offset));}
};
}
