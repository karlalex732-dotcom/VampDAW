#include "stem_state.hpp"
static void stemStateTests(){
    bool reproduced=false;try{Json empty;empty.value("passed",false);}catch(const Json::type_error&){reproduced=true;}
    check(reproduced,"old first-paint null result reproduces JSON type error");
    for(const auto&state:{Json(),Json::object(),Json::array(),Json(17),Json("old"),Json({{"passed","yes"}})}){
        check(!stemFlag(state,"passed"),"idle and malformed result cannot throw or pass");
        check(stemStatusText(false,state,{}).find(L"Ready.")==0,"first paint has ready status for absent result");
        check(stemStatusText(true,state,L"Working")==L"Working","running paint has worker status");
        check(stemIndex(state,"710",1,4)==1,"absent settings use default choice");
    }
    Json prefs={{"710",UINT64_MAX},{"711",-999},{"712","wrong"},{"713",1.9},{"724",1},{"outputRoot",17}};
    check(stemIndex(prefs,"710",0,4)==3&&stemIndex(prefs,"711",0,15)==0,"huge and negative settings cannot index outside options");
    check(stemIndex(prefs,"712",2,4)==2&&stemIndex(prefs,"713",0,5)==0,"wrong preference types use defaults");
    check(stemFlag(prefs,"724",true)&&stemText(prefs,"outputRoot").empty(),"invalid checkbox and path settings use defaults");
    for(const auto&result:{Json(),Json::array(),Json({{"passed","true"}}),Json({{"passed",true}}),Json({{"passed",true},{"sources",Json::array()}}),Json({{"passed",true},{"sources",Json::array({17})}}),Json({{"passed",true},{"sources",Json::array({{{"name","Bass"},{"path","bass.wav"},{"import_path",nullptr}}})}})}){
        bool rejected=false;try{validateStemResult(result);}catch(const std::exception&){rejected=true;}check(rejected,"malformed worker result is rejected before UI and preview");
    }
    validateStemResult({{"passed",false},{"error","Unavailable runtime"}});
    Json good={{"passed",true},{"sources",Json::array({{{"name","Bass"},{"path","bass.wav"}}})}};validateStemResult(good);
    check(stemStatusText(false,good,L"").find(L"Complete")==0,"successful result has completion status");
    check(stemExitMessage(0xC000001DUL).find("CPU instruction")!=std::string::npos&&stemExitMessage(0xC0000135UL).find("DLL")!=std::string::npos,"fatal runtime exits have useful explanations");
    std::ofstream(L"work/test-results/stem-state.json")<<Json({{"passed",true},{"old_first_paint_error_reproduced",reproduced},{"null_and_malformed_states_safe",true},{"preference_bounds",true},{"worker_schema_validation",true}}).dump(2);
}
