#pragma once
#include "plugin_state.hpp"

namespace vamp {
// Legacy byte arrays are encoded while parsing: a large VST state never becomes
// millions of individual JSON values in memory.
class ProjectReader:public nlohmann::json_sax<Json>{
    struct Frame{Json value;std::string key;bool states=false,state=false,bytes=false;std::string encoded;uint8_t tail[3]{};size_t count=0,size=0;};
    std::vector<Frame>stack;
    bool accept(Json value){if(stack.empty()){result=std::move(value);return true;}auto&f=stack.back();if(f.bytes)return fail();if(f.value.is_object())f.value[std::move(f.key)]=std::move(value);else f.value.push_back(std::move(value));return true;}
    bool fail(){error="Invalid VST3 state data";return false;}
    bool number(Json value){if(!stack.empty()&&stack.back().bytes){if(!value.is_number_integer()||value<0||value>255)return fail();auto&f=stack.back();f.tail[f.count++]=value.get<uint8_t>();f.size++;if(f.count==3){appendStateTriplet(f.encoded,f.tail,3);f.count=0;}return true;}return accept(std::move(value));}
    bool begin(bool object){if(stack.size()>=128){error="Project data is nested too deeply";return false;}if(!stack.empty()&&stack.back().bytes)return fail();Frame f;f.value=object?Json::object():Json::array();if(!stack.empty()){auto&parent=stack.back();f.state=object&&parent.states;f.states=!object&&parent.value.is_object()&&parent.key=="pluginStates";f.bytes=!object&&parent.state&&(parent.key=="component"||parent.key=="controller");}stack.push_back(std::move(f));return true;}
    bool finish(){auto f=std::move(stack.back());stack.pop_back();if(f.bytes){if(f.count)appendStateTriplet(f.encoded,f.tail,f.count);f.value=Json{{"encoding","base64"},{"size",f.size},{"data",std::move(f.encoded)}};}return accept(std::move(f.value));}
public:
    Json result;std::string error;
    bool null()override{return accept(nullptr);}bool boolean(bool v)override{return accept(v);}
    bool number_integer(number_integer_t v)override{return number(v);}bool number_unsigned(number_unsigned_t v)override{return number(v);}
    bool number_float(number_float_t v,const string_t&)override{return number(v);}
    bool string(string_t&v)override{return accept(std::move(v));}bool binary(binary_t&v)override{return accept(Json::binary(std::move(v)));}
    bool start_object(std::size_t)override{return begin(true);}bool key(string_t&v)override{stack.back().key=std::move(v);return true;}bool end_object()override{return finish();}
    bool start_array(std::size_t)override{return begin(false);}bool end_array()override{return finish();}
    bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception&)override{error="Invalid project data. Open a backup or recovery copy.";return false;}
};
}
