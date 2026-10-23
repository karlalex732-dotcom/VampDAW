#pragma once
#include "model.hpp"
#include <set>
namespace vamp {
inline void clearTimelineRange(Project&p){p.loopStart=p.loopEnd=0;}
using ClipSelection=std::set<std::pair<int,int>>;
std::pair<double,double> playlistTimeRange(double anchor,double end,int bars,double grid=4);
ClipSelection clipsInRectangle(const Project&project,double beatA,double beatB,double rowA,double rowB);
int cloneTrack(Project&project,int index);
void reorderTrack(Project&project,int from,int to);
void swapPluginSlots(Track&track,int first,int second);
void validateTrackFolders(const Project&project);
int createTrackFolder(Project&project,const std::string&name,int parent=-1);
void assignTracksToFolder(Project&project,const std::set<int>&tracks,int folder);
void moveTrackFolder(Project&project,int folder,int parent);
void dissolveTrackFolder(Project&project,int folder);
void extendPattern(Project&project,int track,int pattern,double end);
// Commits only after validating the complete group; the caller owns the undo snapshot.
ClipSelection transformClips(Project&project,const ClipSelection&selection,double delta,int trackDelta,bool copy);
void makePatternUnique(Project&project,int track,int clip);
void splitArrangementAt(Project&project,double beat);
void spliceArrangementTime(Project&project,double first,double length,bool remove);
}
