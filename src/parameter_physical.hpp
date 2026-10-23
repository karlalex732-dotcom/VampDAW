#pragma once
#include <optional>
#include <cstdlib>
#include <cctype>
#include <cmath>
namespace vamp {
inline std::string physicalUnit(std::string text){std::string result;for(unsigned char c:text)if(std::isalnum(c)||c=='%')result+=(char)std::tolower(c);return result;}
inline std::optional<double>physicalReadout(std::string text,const std::string&unit){
    for(char&c:text)if(c==',')c='.';size_t at=text.find_first_of("-+.0123456789");if(at==std::string::npos)return{};char*end=nullptr;double value=std::strtod(text.c_str()+at,&end);if(end==text.c_str()+at||!std::isfinite(value))return{};auto suffix=physicalUnit(end);auto target=physicalUnit(unit);
    if(target=="hz"&&(suffix=="khz"||suffix=="k"))value*=1000;if(target=="ms"&&(suffix=="s"||suffix=="sec"))value*=1000;return value;
}
}
