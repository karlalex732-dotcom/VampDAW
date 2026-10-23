#pragma once
#include "model.hpp"
#include <cwctype>
#include <set>
namespace vamp {
inline std::wstring favoriteKey(const std::string&path){auto key=fs::path(wide(path)).lexically_normal().wstring();std::replace(key.begin(),key.end(),L'/',L'\\');std::transform(key.begin(),key.end(),key.begin(),[](wchar_t c){return(wchar_t)towlower(c);});return key;}
inline bool pluginIsStarred(const Json&favorites,const std::string&path){auto key=favoriteKey(path);for(auto&e:favorites)if(e.is_object()&&e.contains("path")&&e["path"].is_string()&&favoriteKey(e["path"].get<std::string>())==key)return true;return false;}
inline Json cleanFavorites(const Json&input){Json result=Json::array();std::set<std::wstring>seen;if(!input.is_array())return result;for(auto&e:input){if(!e.is_object()||!e.contains("path")||!e["path"].is_string())continue;auto path=e["path"].get<std::string>();auto key=favoriteKey(path);auto ext=fs::path(key).extension().wstring();if(path.empty()||(ext!=L".vst3"&&ext!=L".dll"&&ext!=L".vampfx")||!seen.insert(key).second)continue;Json entry={{"path",path},{"name",e.contains("name")&&e["name"].is_string()?e["name"].get<std::string>():utf8(fs::path(wide(path)).stem().wstring())},{"instrument",e.contains("instrument")&&e["instrument"].is_boolean()?e["instrument"].get<bool>():false},{"vamp",e.contains("vamp")&&e["vamp"].is_boolean()?e["vamp"].get<bool>():false}};result.push_back(entry);if(result.size()>=4096)break;}return result;}
inline void togglePluginStar(Json&favorites,Json entry){auto key=favoriteKey(entry.value("path",""));for(auto i=favorites.begin();i!=favorites.end();++i)if(favoriteKey(i->value("path",""))==key){favorites.erase(i);return;}auto normalized=cleanFavorites(Json::array({entry}));if(!normalized.empty())favorites.push_back(normalized.front());}
inline std::vector<Json>pluginPickerEntries(int tab,const std::vector<Json>&native,const Json&catalog,const Json&favorites,bool effectsOnly,bool instrumentsOnly,const std::wstring&search){
    std::vector<Json>result;std::set<std::wstring>seen;auto lower=[](std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return(wchar_t)towlower(c);});return s;};auto needle=lower(search);
    auto add=[&](const Json&e){if(!e.is_object()||!e.contains("path")||!e["path"].is_string())return;bool instrument=e.value("instrument",false);if((effectsOnly&&instrument)||(instrumentsOnly&&!instrument))return;auto text=lower(wide(e.value("name",""))+L" "+wide(e.value("vendor",""))+L" "+wide(e.value("category","")));if(!needle.empty()&&text.find(needle)==std::wstring::npos)return;if(seen.insert(favoriteKey(e["path"].get<std::string>())).second)result.push_back(e);};
    if(tab==0){for(auto&e:native)add(e);for(auto&e:catalog)if(e.value("ok",false)&&e.value("vamp",false))add(e);}
    else if(tab==1){for(auto&e:catalog)if(e.value("ok",false)&&!e.value("vamp",false))add(e);}
    else for(auto&e:favorites){Json current=e;auto key=favoriteKey(e.value("path",""));for(auto&candidate:native)if(favoriteKey(candidate.value("path",""))==key)current=candidate;for(auto&candidate:catalog)if(candidate.value("ok",false)&&favoriteKey(candidate.value("path",""))==key)current=candidate;add(current);}
    std::stable_sort(result.begin(),result.end(),[&](auto&a,auto&b){return lower(wide(a.value("name","")))<lower(wide(b.value("name","")));});return result;
}
}
