#pragma once
#include "model.hpp"
#include <set>
namespace vamp {
// relative() returns an empty path without an error for different Windows drives.
inline std::string projectMediaPath(const fs::path&file,const fs::path&destination){if(file.empty())return{};std::error_code ec;auto absolute=fs::absolute(file,ec);if(ec)absolute=file;auto relative=fs::relative(absolute,destination.parent_path(),ec);return utf8((ec||relative.empty()?absolute:relative).wstring());}
struct MissingMedia{std::string owner;std::shared_ptr<Sample>sample;};
inline std::vector<MissingMedia>missingProjectMedia(const Project&p){std::vector<MissingMedia>out;std::set<const Sample*>seen;auto add=[&](const std::shared_ptr<Sample>&s,const std::string&owner){if(s&&s->missing&&seen.insert(s.get()).second)out.push_back({owner,s});};auto track=[&](const Track&t){add(t.sample,t.name);for(auto&c:t.clips){add(c.audio,t.name+" / "+c.name);add(c.warpReference,t.name+" / Warp reference");}};for(auto&t:p.tracks)track(t);track(p.master);return out;}
inline void relinkProjectMedia(Project&p,std::shared_ptr<Sample>old,std::shared_ptr<Sample>replacement){auto track=[&](Track&t){if(t.sample==old)t.sample=replacement;for(auto&c:t.clips){if(c.audio==old)c.audio=replacement;if(c.warpReference==old)c.warpReference=replacement;c.warpAudio.reset();c.warpKey=0;}};for(auto&t:p.tracks)track(t);track(p.master);for(auto&v:p.versions)if(v.snapshot){v.snapshot=std::make_shared<Project>(*v.snapshot);relinkProjectMedia(*v.snapshot,old,replacement);}if(p.abWorking){p.abWorking=std::make_shared<Project>(*p.abWorking);relinkProjectMedia(*p.abWorking,old,replacement);}}
}
