#pragma once
#include <windows.h>
namespace vamp {
inline HMODULE pluginUiModule(){HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&pluginUiModule,&module);return module;}
#ifdef ORACLE_RACK_BUILD
inline constexpr const char*pluginHostName="Oracle Rack";
inline constexpr const char16_t*pluginHostName16=u"Oracle Rack";
inline constexpr const wchar_t*pluginHostNameW=L"Oracle Rack";
inline constexpr const wchar_t*nativeEffectClass=L"OracleRack.NativeEffect";
inline constexpr const wchar_t*hostedVst3Class=L"OracleRack.Plugin";
inline constexpr const wchar_t*hostedVst2Class=L"OracleRack.VST2";
inline constexpr const wchar_t*pluginPromptClass=L"OracleRack.TextPrompt";
#else
inline constexpr const char*pluginHostName="VampDAW";
inline constexpr const char16_t*pluginHostName16=u"VampDAW";
inline constexpr const wchar_t*pluginHostNameW=L"VampDAW";
inline constexpr const wchar_t*nativeEffectClass=L"VampDAW.NativeEffect";
inline constexpr const wchar_t*hostedVst3Class=L"VampDAW.Plugin";
inline constexpr const wchar_t*hostedVst2Class=L"VampDAW.VST2";
inline constexpr const wchar_t*pluginPromptClass=L"VampDAW.TextPrompt";
#endif
}
