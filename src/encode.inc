const wchar_t*audioExtension(AudioFormat f){static const wchar_t*names[]={L"wav",L"mp3",L"flac",L"ogg",L"m4a",L"opus",L"aiff"};return names[(int)f];}
const wchar_t*audioFormatName(AudioFormat f){static const wchar_t*names[]={L"WAV",L"MP3",L"FLAC",L"OGG Vorbis",L"AAC / M4A",L"Opus",L"AIFF"};return names[(int)f];}
#include "encoder_path.inc"
static std::wstring quoteArgument(const std::wstring&value){std::wstring out=L"\"";int slashes=0;for(wchar_t c:value){if(c==L'\\'){slashes++;continue;}if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');slashes=0;out+=c;}out.append(slashes*2,L'\\');return out+L"\"";}
void renderAudio(const RenderPlan&p,const fs::path&path,const ExportOptions&o,ExportControl*control){
    if(o.format==AudioFormat::Wav){renderWav(p,path,o,control);return;}
    auto encoder=audioEncoderPath();if(!fs::exists(encoder))throw std::runtime_error("Audio encoder is missing: tools/ffmpeg/bin/ffmpeg.exe");
    static std::atomic<unsigned>serial{0};auto stem=path;stem+=L".render-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial++);auto source=stem;source+=L".wav";auto target=stem;target+=L"."+std::wstring(audioExtension(o.format));auto progress=stem;progress+=L".progress";auto log=stem;log+=L".log";
    PROCESS_INFORMATION process{};HANDLE logHandle=INVALID_HANDLE_VALUE;
    auto cleanup=[&]{if(process.hProcess)CloseHandle(process.hProcess);if(process.hThread)CloseHandle(process.hThread);if(logHandle!=INVALID_HANDLE_VALUE)CloseHandle(logHandle);std::error_code ec;for(auto&file:{source,target,progress,log})fs::remove(file,ec);if(control)control->renderWeight=1;};
    try{
        ExportOptions stage=o;bool lossless=o.format==AudioFormat::Flac||o.format==AudioFormat::Aiff;stage.bits=lossless?(o.bits==16?16:24):32;if(!lossless)stage.dither=false;if(control){control->renderWeight=.75;control->phase=0;}renderWav(p,source,stage,control);if(control&&control->cancel)throw std::runtime_error("Export cancelled");
        std::vector<std::wstring>args={L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-y",L"-i",source.wstring(),L"-map",L"0:a:0",L"-vn",L"-progress",progress.wstring()};
        auto add=[&](const wchar_t*k,std::wstring v){args.push_back(k);args.push_back(std::move(v));};
        int bitrate=(int)clamp(o.bitrate,64,320);switch(o.format){
        case AudioFormat::Mp3:add(L"-c:a",L"libmp3lame");add(L"-b:a",std::to_wstring(bitrate)+L"k");if(o.rate>48000)add(L"-ar",L"48000");break;
        case AudioFormat::Flac:add(L"-c:a",L"flac");add(L"-compression_level",L"5");break;
        case AudioFormat::Ogg:add(L"-c:a",L"libvorbis");add(L"-q:a",std::to_wstring((int)clamp(o.quality,0,10)));break;
        case AudioFormat::Aac:add(L"-c:a",L"aac");add(L"-b:a",std::to_wstring(bitrate)+L"k");add(L"-movflags",L"+faststart");if(o.rate>96000)add(L"-ar",L"96000");break;
        case AudioFormat::Opus:add(L"-c:a",L"libopus");add(L"-b:a",std::to_wstring(bitrate)+L"k");add(L"-ar",L"48000");break;
        case AudioFormat::Aiff:add(L"-c:a",stage.bits==16?L"pcm_s16be":L"pcm_s24be");break;default:break;}
        args.push_back(target.wstring());std::wstring command=quoteArgument(encoder.wstring());for(auto&arg:args)command+=L" "+quoteArgument(arg);
        SECURITY_ATTRIBUTES security{sizeof security,nullptr,TRUE};logHandle=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(logHandle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create encoder log");
        STARTUPINFOW startup{sizeof startup};startup.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;startup.wShowWindow=SW_HIDE;startup.hStdError=logHandle;startup.hStdOutput=logHandle;startup.hStdInput=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        BOOL started=CreateProcessW(encoder.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);if(startup.hStdInput!=INVALID_HANDLE_VALUE)CloseHandle(startup.hStdInput);if(!started)throw std::runtime_error("Cannot start audio encoder");if(control)control->phase=1;
        double duration=std::max(.001,(fs::file_size(source)-44)/double(stage.rate*stage.channels*(stage.bits/8)));
        for(;;){DWORD wait=WaitForSingleObject(process.hProcess,60);if(control&&control->cancel){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,2000);throw std::runtime_error("Export cancelled");}if(wait==WAIT_OBJECT_0)break;if(wait==WAIT_FAILED){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,2000);throw std::runtime_error("Encoder wait failed");}if(control){std::ifstream report(progress);std::string line;double seconds=0;while(std::getline(report,line))if(line.rfind("out_time_us=",0)==0)try{seconds=std::stod(line.substr(12))/1000000.;}catch(...){}control->progress=.75+.24*clamp(seconds/duration,0,1);}}
        DWORD exit=1;GetExitCodeProcess(process.hProcess,&exit);if(exit){std::ifstream error(log);std::string message((std::istreambuf_iterator<char>(error)),{});throw std::runtime_error("Audio encoder: "+message.substr(0,1800));}if(control&&control->cancel)throw std::runtime_error("Export cancelled");if(!fs::exists(target)||fs::file_size(target)==0)throw std::runtime_error("Encoder produced an empty file");if(!MoveFileExW(target.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace export output");cleanup();if(control)control->progress=1;
    }catch(...){cleanup();throw;}
}
