#pragma once
#include "theme_data.hpp"
#include "settings_store.hpp"
static void themeTests(){
    ThemeData theme;theme.name=utf8(L"Моя ночь — Karsei");theme.colors={0x000000,0x101018,0x222233,0xff0044,0x88ffcc,0xffffff,0xaabbcc,0x202020,0x445566};
    auto file=fs::path(L"work/test-results/theme-roundtrip.vamptheme");{std::ofstream out(file);out<<themeJson(theme).dump(2);}Json saved;{std::ifstream in(file);in>>saved;}auto restored=readTheme(saved);check(restored.name==theme.name&&restored.colors==theme.colors,"theme file preserves Unicode name and all nine colors");check(themeColor("#aB01fF")==0xab01ff,"case insensitive HEX color entry");
    for(const auto&value:{"#12ff","1234567","#gg0000","#11223344"}){bool rejected=false;try{themeColor(value);}catch(...){rejected=true;}check(rejected,"invalid HEX color rejected");}
    for(int mode=0;mode<3;mode++){auto malformed=saved;if(mode==0)malformed["version"]=2;if(mode==1)malformed["colors"].erase("grid");if(mode==2)malformed["colors"]["accent"]=123;bool rejected=false;try{readTheme(malformed);}catch(...){rejected=true;}check(rejected,"invalid theme import rejected before application");}
    auto settings=fs::path(L"work/test-results/custom-theme-settings.json");SettingsStore first;auto previous=first.load(settings);first.initialize(previous);first.save(settings,{{"theme",7},{"customTheme",themeJson(theme)}});SettingsStore restart;auto j=restart.load(settings);check(j.at("theme")==7&&readTheme(j.at("customTheme")).colors==theme.colors,"custom theme survives settings restart");
    std::ofstream(L"work/test-results/custom-theme.json")<<Json({{"passed",true},{"colors",9},{"unicode_name",true},{"file_round_trip",true},{"settings_restart",true},{"malformed_import_rejected",true}}).dump(2);
}
