#pragma once
#include "model.hpp"
#include <stdexcept>

namespace vamp {
inline constexpr char StateAlphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
inline void appendStateTriplet(std::string&out,const uint8_t*bytes,size_t count){
    unsigned v=unsigned(bytes[0])<<16;if(count>1)v|=unsigned(bytes[1])<<8;if(count>2)v|=bytes[2];
    out+=StateAlphabet[(v>>18)&63];out+=StateAlphabet[(v>>12)&63];out+=count>1?StateAlphabet[(v>>6)&63]:'=';out+=count>2?StateAlphabet[v&63]:'=';
}
inline Json encodePluginStateBytes(const std::vector<uint8_t>&bytes){
    std::string text;text.reserve((bytes.size()+2)/3*4);for(size_t i=0;i<bytes.size();i+=3)appendStateTriplet(text,bytes.data()+i,std::min<size_t>(3,bytes.size()-i));
    return Json{{"encoding","base64"},{"size",bytes.size()},{"data",std::move(text)}};
}
inline std::vector<uint8_t>decodePluginStateBytes(const Json&j){
    if(j.is_array()){std::vector<uint8_t>out;out.reserve(j.size());for(auto&v:j){if(!v.is_number_integer()||v<0||v>255)throw std::runtime_error("Invalid VST3 state data");out.push_back(v.get<uint8_t>());}return out;}
    if(!j.is_object()||j.value("encoding","")!="base64"||!j.contains("data")||!j["data"].is_string())throw std::runtime_error("Invalid VST3 state data");
    auto&text=j["data"].get_ref<const std::string&>();if(text.size()%4)throw std::runtime_error("Invalid VST3 state data");
    size_t padding=text.empty()?0:text.back()=='='?(text.size()>1&&text[text.size()-2]=='='?2:1):0;
    size_t size=text.size()/4*3-padding;if(!j.contains("size")||!j["size"].is_number_integer()||j["size"]<0||j["size"].get<uint64_t>()!=size)throw std::runtime_error("Invalid VST3 state size");
    std::vector<uint8_t>out;out.reserve(size);auto digit=[](char c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};
    for(size_t i=0;i<text.size();i+=4){unsigned v=0;for(size_t n=0;n<4;n++){bool pad=i+n>=text.size()-padding;int value=pad&&text[i+n]=='='?0:digit(text[i+n]);if(value<0||(!pad&&text[i+n]=='='))throw std::runtime_error("Invalid VST3 state data");v=(v<<6)|value;}out.push_back((v>>16)&255);if(out.size()<size)out.push_back((v>>8)&255);if(out.size()<size)out.push_back(v&255);}
    return out;
}
inline void compactPluginState(Json&state){if(state.is_object())for(auto key:{"component","controller"})if(state.contains(key)&&state[key].is_array())state[key]=encodePluginStateBytes(decodePluginStateBytes(state[key]));}
}
