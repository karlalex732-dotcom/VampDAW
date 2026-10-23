#include "aggregate.hpp"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <avrt.h>
namespace vamp {
AggregateOutput::AggregateOutput(){stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);}
AggregateOutput::~AggregateOutput(){stop();CloseHandle(stopEvent);}
void AggregateOutput::start(const std::wstring&id,int rate){stop();buffer.reset(std::max(512,rate/30));state=0;ResetEvent(stopEvent);worker=std::thread([this,id,rate]{run(id,rate);});}
void AggregateOutput::stop(){if(worker.joinable()){SetEvent(stopEvent);worker.join();}state=0;}
void AggregateOutput::run(std::wstring id,int rate){CoInitializeEx(nullptr,COINIT_MULTITHREADED);DWORD index=0;HANDLE task=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);IMMDeviceEnumerator*enumerator=nullptr;IMMDevice*device=nullptr;IAudioClient*client=nullptr;IAudioRenderClient*render=nullptr;HANDLE ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    auto cleanup=[&]{if(client)client->Stop();if(render)render->Release();if(client)client->Release();if(device)device->Release();if(enumerator)enumerator->Release();CloseHandle(ready);if(task)AvRevertMmThreadCharacteristics(task);CoUninitialize();};
    HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)&enumerator);if(SUCCEEDED(hr))hr=enumerator->GetDevice(id.c_str(),&device);if(SUCCEEDED(hr))hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)&client);
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;format.nChannels=2;format.nSamplesPerSec=rate;format.wBitsPerSample=32;format.nBlockAlign=8;format.nAvgBytesPerSec=rate*8;
    if(SUCCEEDED(hr))hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,0,0,&format,nullptr);UINT32 capacity=0;if(SUCCEEDED(hr))hr=client->GetBufferSize(&capacity);if(SUCCEEDED(hr))hr=client->SetEventHandle(ready);if(SUCCEEDED(hr))hr=client->GetService(__uuidof(IAudioRenderClient),(void**)&render);if(SUCCEEDED(hr)){latencyMs=capacity*1000./rate+1000./30;hr=client->Start();}if(FAILED(hr)){state=-1;cleanup();return;}state=1;
    HANDLE waits[]={stopEvent,ready};DWORD wait=0;std::array<float,MaxBlock*2>samples{};while((wait=WaitForMultipleObjects(2,waits,FALSE,2000))==WAIT_OBJECT_0+1){UINT32 padding=0;hr=client->GetCurrentPadding(&padding);if(FAILED(hr))break;UINT32 frames=capacity-padding;if(!frames)continue;BYTE*data=nullptr;hr=render->GetBuffer(frames,&data);if(FAILED(hr))break;for(UINT32 at=0;at<frames;){int n=(int)std::min<UINT32>(MaxBlock,frames-at);buffer.read(samples.data(),n);memcpy(data+at*8,samples.data(),n*8);at+=n;}hr=render->ReleaseBuffer(frames,0);if(FAILED(hr))break;}state=FAILED(hr)||wait==WAIT_TIMEOUT||wait==WAIT_FAILED?-4:0;cleanup();
}
}
