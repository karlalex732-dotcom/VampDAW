#pragma once
#include "model.hpp"
#include <array>
#include <cstdio>

namespace vamp {
struct ThemeData {
    std::string name="My Night";
    // Background, panel, raised surface, accent, secondary, text, muted text, grid, bar grid.
    std::array<uint32_t,9> colors={0x0e0e14,0x16151d,0x251e29,0xd74362,0x9978d7,0xeee7f1,0x938599,0x302932,0x4c2e41};
};
inline std::string themeHex(uint32_t color){char value[8];std::snprintf(value,sizeof value,"#%06X",color&0xffffff);return value;}
inline uint32_t themeColor(const std::string&value){
    if(value.size()!=7||value[0]!='#')throw std::runtime_error("Use a color in #RRGGBB format");
    uint32_t result=0;for(size_t i=1;i<value.size();i++){char c=value[i];int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(digit<0)throw std::runtime_error("Use a color in #RRGGBB format");result=(result<<4)|digit;}return result;
}
inline const std::array<const char*,9>&themeKeys(){static const std::array<const char*,9>keys={"background","panel","raised","accent","secondary","text","muted","grid","barGrid"};return keys;}
inline Json themeJson(const ThemeData&theme){Json colors=Json::object();for(size_t i=0;i<theme.colors.size();i++)colors[themeKeys()[i]]=themeHex(theme.colors[i]);return {{"format","VampDAW Theme"},{"version",1},{"name",theme.name},{"colors",colors}};}
inline ThemeData readTheme(const Json&value){
    if(!value.is_object()||value.value("format",std::string())!="VampDAW Theme"||value.value("version",0)!=1)throw std::runtime_error("Invalid VampDAW theme file");
    ThemeData out;out.name=value.at("name").get<std::string>();if(out.name.empty()||out.name.size()>256)throw std::runtime_error("Theme name must contain 1 to 256 bytes");
    const auto&colors=value.at("colors");if(!colors.is_object())throw std::runtime_error("Invalid theme colors");for(size_t i=0;i<out.colors.size();i++)out.colors[i]=themeColor(colors.at(themeKeys()[i]).get<std::string>());return out;
}
}
