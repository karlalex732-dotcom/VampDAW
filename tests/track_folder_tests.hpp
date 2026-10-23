#include "playlist_layout.hpp"
static void trackFolderTests(){
    Project p;p.tracks.resize(4);p.tracks[0].name="Kick";p.tracks[1].name="Snare";p.tracks[2].name="Bass";
    int drums=createTrackFolder(p,"Drums"),song=createTrackFolder(p,"Song"),layers=createTrackFolder(p,"Layers",drums);
    assignTracksToFolder(p,{0,1},layers);assignTracksToFolder(p,{2},song);moveTrackFolder(p,drums,song);
    check(p.tracks[0].folder==layers&&p.tracks[1].folder==layers&&p.folders[0].parent==song,"selected tracks group into recursively nested folders");
    auto snapshot=projectJson(p,L"work/test-results/folders.vamp");bool cycle=false;try{moveTrackFolder(p,song,layers);}catch(...){cycle=true;}
    check(cycle&&projectJson(p,L"work/test-results/folders.vamp")==snapshot,"folder cycle rejected without changing the project");
    p.tracks[0].playlistHeight=132;p.tracks[1].playlistHeight=28;
    p.folders[0].collapsed=true;auto restored=projectFromJson(projectJson(p,L"work/test-results/folders.vamp"),L"work/test-results/folders.vamp");
    check(restored.tracks[0].playlistHeight==132&&restored.tracks[1].playlistHeight==28&&restored.tracks[2].playlistHeight==0,"independent track heights and automatic height persist");
    PlaylistLayout layout;layout.reset({32,132,28,60,24},180);check(layout.row(31,0)<1&&layout.row(32,0)==1&&layout.row(164,0)==2,"variable track boundaries map to correct rows for clips and drops");
    for(int offset=0;offset<=layout.maxOffset();offset++)for(int row=offset;row<layout.count();row++){double position=row+.37;check(std::abs(layout.row(layout.y(position,offset),offset)-position)<1e-5,"selection anchor and hit testing agree after scrolling variable heights");}
    check(layout.y(layout.count(),layout.maxOffset())<=layout.viewport&&layout.height(1)==132,"last track can be fully scrolled into view without altering neighboring heights");
    check(restored.folders.size()==3&&restored.folders[0].parent==song&&restored.folders[0].collapsed&&restored.tracks[1].folder==layers,"nested folders, membership and collapse state persist");
    int copy=cloneTrack(restored,0);check(restored.tracks[copy].folder==layers,"cloned track retains folder membership");reorderTrack(restored,copy,0);check(restored.tracks[0].folder==layers&&restored.tracks[0].name=="Kick / Copy","reordered track retains folder membership");
    auto count=restored.tracks.size();dissolveTrackFolder(restored,drums);check(restored.tracks.size()==count&&restored.folders.size()==2&&restored.folders[1].parent==song,"dissolving parent preserves tracks and reparents nested folders");
    dissolveTrackFolder(restored,layers);check(restored.tracks[0].folder==song&&restored.tracks[1].folder==song&&restored.tracks.size()==count,"dissolving child preserves tracks and assigns them to parent");
    bool invalid=false;try{assignTracksToFolder(restored,{0,512},song);}catch(...){invalid=true;}check(invalid,"invalid group assignment rejected");
    auto old=Project();old.tracks.resize(2);auto legacy=projectJson(old,L"work/test-results/legacy-five.vamp");legacy["version"]=5;legacy.erase("folders");for(auto&t:legacy["tracks"])t["output"]=256;legacy["tracks"][0]["sends"]=Json::array({{{"target",256},{"gain",.2}}});
    auto migrated=projectFromJson(legacy,L"work/test-results/legacy-five.vamp");check(migrated.tracks[0].output==MasterChannel&&migrated.tracks[0].sends[0].target==MasterChannel&&migrated.folders.empty(),"legacy master index 256 migrates to 512 for outputs and sends");
    Project modern;modern.tracks.resize(257);modern.tracks[0].output=256;auto modernRestored=projectFromJson(projectJson(modern,L"work/test-results/modern.vamp"),L"work/test-results/modern.vamp");check(modernRestored.tracks[0].output==256,"new project can route to real track 257 without confusing it with master");
    auto malformed=snapshot;malformed["folders"][0]["parent"]=layers;bool rejected=false;try{projectFromJson(malformed,L"work/test-results/folders.vamp");}catch(...){rejected=true;}check(rejected,"cyclic folder data rejected when loading");
    Json report={{"passed",true},{"track_limit",TrackCount},{"nested_folders",true},{"group_assignment",true},{"cycles_rejected_atomically",true},{"membership_follows_clone_and_reorder",true},{"dissolve_preserves_tracks",true},{"round_trip",true},{"legacy_master_migration",true},{"new_track_257_routing",true},{"individual_track_heights",true},{"variable_height_hit_testing_and_scrolling",true}};
    std::ofstream(L"work/test-results/track-folders.json")<<report.dump(2);
}
