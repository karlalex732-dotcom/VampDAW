#include "dsp_meter.hpp"
static void dspMeterTests(){
    DspMeter a,b;a.update(.0005,128,48000);b.update(.004,1024,48000);check(std::abs(a.average-b.average)<1e-10&&std::abs(a.average-18.75)<1e-10,"DSP divides work time by the actual audio deadline");
    a.reset();b.reset();a.update(0,128,48000);b.update(0,1024,48000);for(int i=0;i<80;i++)a.update(.00128,128,48000);for(int i=0;i<10;i++)b.update(.01024,1024,48000);check(std::abs(a.average-b.average)<1e-10,"DSP smoothing follows audio time and does not depend on buffer size");
    DspMeter rate;rate.update(.001,128,96000);check(std::abs(rate.average-75)<1e-10,"DSP reflects the actual sample rate");rate.reset();check(rate.average==0&&rate.peak==0,"DSP values reset after audio reconfiguration");
    check(sharedAudioPeriod(128,480,16,64,480)==128&&sharedAudioPeriod(129,480,16,64,480)==144&&sharedAudioPeriod(1024,480,16,64,480)==480,"WASAPI applies requested periods and reports driver limits");
    std::array<float,32>interleaved{},decoded{};for(int i=0;i<32;i++)interleaved[i]=(i%7-3)*.4f;
    for(int format:{0,1,2,3,4,16,17,18,19,20,24,25,26,27})for(int channel=0;channel<2;channel++){
        std::array<uint8_t,256>fast{},reference{};asioWriteBlock(fast.data(),interleaved.data(),channel,16,format);for(int i=0;i<16;i++)asioWriteSample(reference.data(),i,format,interleaved[i*2+channel]);check(fast==reference,"fast ASIO block conversion preserves every output bit");asioReadBlock(fast.data(),decoded.data(),16,format);for(int i=0;i<16;i++)check(std::abs(decoded[i]-asioReadSample(reference.data(),i,format))<1e-7,"fast ASIO block conversion preserves input samples");
    }
    Json report={{"passed",true},{"actual_deadline_normalization",true},{"time_based_smoothing",true},{"reset_on_reconfiguration",true},{"wasapi_requested_period",true},{"asio_block_conversion_bit_identical",true}};std::ofstream(L"work/test-results/dsp-meter.json")<<report.dump(2);
}
