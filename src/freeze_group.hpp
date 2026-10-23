#pragma once
#include "model.hpp"
#include <set>
namespace vamp {
inline std::set<int>folderMembers(const Project&p,int folder){std::set<int>result;if(folder<0)return result;for(int i=0;i<(int)p.tracks.size();i++){int parent=p.tracks[i].folder;std::set<int>visited;while(parent>=0&&visited.insert(parent).second){if(parent==folder){result.insert(i);break;}auto it=std::find_if(p.folders.begin(),p.folders.end(),[&](const auto&f){return f.id==parent;});if(it==p.folders.end())break;parent=it->parent;}}return result;}
inline std::vector<int>folderFreezeOrder(const Project&p,int folder,const std::vector<int>&signalOrder){auto members=folderMembers(p,folder);std::vector<int>result;for(int id:signalOrder)if(id>=0&&id<(int)p.tracks.size()&&members.count(id)&&!p.tracks[id].frozen&&!p.tracks[id].external)result.push_back(id);return result;}
}
