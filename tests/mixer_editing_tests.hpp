#include "editing.hpp"
static void mixerEditingTests(){
    Project p;p.tracks.resize(1);
    for(int channel:{0,MasterChannel}){
        auto&t=channel==MasterChannel?p.master:p.tracks[0];
        for(int slot=0;slot<16;slot++){t.pluginPaths.push_back("effect-"+std::to_string(slot)+".vampfx");t.pluginStates.push_back({{"identity",slot}});t.pluginBypass.push_back(slot==15);}
        t.automationTarget=2;t.automationSlot=0;t.automationParam=913;t.automationName="1 / Effect A / Cutoff";t.automationEnabled=true;t.automation={{0,.2f},{4,.8f}};
        t.automationLanes={{2,15,207,"16 / Effect B / Mix",true,{{0,.7f}}},{2,7,314,"Custom automation name",false,{{2,.4f}}},{0,-1,0,"Volume",true,{{0,.6f}}}};
        auto original=t;swapPluginSlots(t,0,15);
        check(t.pluginPaths[15]==original.pluginPaths[0]&&t.pluginPaths[0]==original.pluginPaths[15],"non-adjacent FX slots swap on channel and master");
        check(t.pluginStates[15]["identity"]==0&&t.pluginStates[0]["identity"]==15&&t.pluginBypass[0]&&!t.pluginBypass[15],"FX identity, state and bypass follow the effect");
        check(t.automationSlot==15&&t.automationName=="16 / Effect A / Cutoff"&&t.automationParam==913&&t.automation[1].value==.8f&&t.automationEnabled,"active parameter automation follows swapped effect");
        check(t.automationLanes[0].slot==0&&t.automationLanes[0].name=="1 / Effect B / Mix"&&t.automationLanes[0].parameter==207&&t.automationLanes[0].points[0].value==.7f,"saved parameter automation follows swapped effect");
        check(t.automationLanes[1].slot==7&&t.automationLanes[1].name=="Custom automation name"&&t.automationLanes[2].target==0,"other automation stays attached to its original target");
        auto snapshot=projectJson(p,L"work/test-results/mixer-slots.vamp");bool rejected=false;try{swapPluginSlots(t,1,16);}catch(...){rejected=true;}
        check(rejected&&projectJson(p,L"work/test-results/mixer-slots.vamp")==snapshot,"invalid slot swap leaves project unchanged");
        auto restored=projectFromJson(snapshot,L"work/test-results/mixer-slots.vamp");auto&saved=channel==MasterChannel?restored.master:restored.tracks[0];
        check(saved.pluginStates[15]["identity"]==0&&saved.automationSlot==15&&saved.automationLanes[0].slot==0,"reordered rack and automation survive project round trip");
        swapPluginSlots(t,0,15);check(t.pluginPaths==original.pluginPaths&&t.pluginStates==original.pluginStates&&t.pluginBypass==original.pluginBypass&&t.automationSlot==0,"reverse FX swap restores chain state");
    }
    Json report={{"passed",true},{"channel_and_master",true},{"non_adjacent_slots",true},{"state_bypass_and_parameter_automation_preserved",true},{"invalid_edit_is_atomic",true},{"project_round_trip",true}};
    std::ofstream(L"work/test-results/mixer-editing.json")<<report.dump(2);
}
