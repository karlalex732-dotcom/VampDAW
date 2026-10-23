#include "localization.hpp"
static void localizationTests(){
    ui::language=0;
    for(auto&row:ui::english){auto result=ui::translate(row.source);for(wchar_t c:result)check(c<0x400||c>0x4ff,"English translation must not leak Russian text");}
    check(ui::translate(L"Проверено 17 / 39 модулей.\nUser Sounds: WAV, FLAC, MP3 (кодеки Windows).")==L"Scanned 17 / 39 modules.\nUser Sounds: WAV, FLAC, MP3 (Windows codecs).","dynamic English labels preserve numbers and file formats");
    check(ui::translate(L"Language / Язык")==L"Language","language control localizes itself");
    ui::language=1;check(ui::translate(L"SOURCE / INSTRUMENT")==L"ИСТОЧНИК / ИНСТРУМЕНТ","Russian source panel");
    check(ui::translate(L"Save as…")==L"Сохранить как…","Russian session menu");
    check(ui::translate(L"2 bars before recording")==L"2 такта перед записью","Russian two-bar count-in");
    check(ui::translate(L"The track input device is disconnected. Choose Audio Input again.").find(L"отключено")!=std::wstring::npos,"Russian audio error");
    ui::language=0;check(ui::translate(L"Omnisphere 2 / Kontakt 8 / Serum2")==L"Omnisphere 2 / Kontakt 8 / Serum2","third-party plugin names stay unchanged");ui::language=1;
}
