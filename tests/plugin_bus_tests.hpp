#include "vst_audio_buses.hpp"

namespace {
class BusTestPlugin : public Steinberg::Vst::IComponent, public Steinberg::Vst::IAudioProcessor {
    using SBResult = Steinberg::tresult;
public:
    using Info = Steinberg::Vst::BusInfo;
    using Arrangement = Steinberg::Vst::SpeakerArrangement;
    std::vector<Info> ins, outs;
    std::vector<Arrangement> inArr, outArr;
    std::vector<bool> inActive, outActive;
    bool nativeRequest = false, adaptRequest = false, setup = false, activationBeforeSetup = false;
    static Info bus(int channels, int type = Steinberg::Vst::kMain) {
        Info result{}; result.channelCount = channels; result.busType = type; return result;
    }
    static Arrangement arrangement(int n) {
        return n == 1 ? Steinberg::Vst::SpeakerArr::kMono : n == 64 ? ~Arrangement(0) : (Arrangement(1) << n) - 1;
    }
    void layouts(std::vector<Info> inputs, std::vector<Info> outputs) {
        ins = std::move(inputs); outs = std::move(outputs); inArr.clear(); outArr.clear();
        for (auto& b : ins) inArr.push_back(arrangement(b.channelCount));
        for (auto& b : outs) outArr.push_back(arrangement(b.channelCount));
        inActive.assign(ins.size(), false); outActive.assign(outs.size(), false);
        setup = activationBeforeSetup = false;
    }
    SBResult PLUGIN_API queryInterface(const Steinberg::TUID, void** object) override { *object = nullptr; return Steinberg::kNoInterface; }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    SBResult PLUGIN_API initialize(Steinberg::FUnknown*) override { return 0; }
    SBResult PLUGIN_API terminate() override { return 0; }
    SBResult PLUGIN_API getControllerClassId(Steinberg::TUID) override { return Steinberg::kNoInterface; }
    SBResult PLUGIN_API setIoMode(Steinberg::Vst::IoMode) override { return 0; }
    Steinberg::int32 PLUGIN_API getBusCount(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection dir) override {
        return type == Steinberg::Vst::kAudio ? (int)(dir ? outs.size() : ins.size()) : 0;
    }
    SBResult PLUGIN_API getBusInfo(Steinberg::Vst::MediaType, Steinberg::Vst::BusDirection dir, Steinberg::int32 index, Info& info) override {
        info = (dir ? outs : ins).at(index); return 0;
    }
    SBResult PLUGIN_API getRoutingInfo(Steinberg::Vst::RoutingInfo&, Steinberg::Vst::RoutingInfo&) override { return Steinberg::kNotImplemented; }
    SBResult PLUGIN_API activateBus(Steinberg::Vst::MediaType, Steinberg::Vst::BusDirection dir, Steinberg::int32 index, Steinberg::TBool active) override {
        activationBeforeSetup |= !setup; (dir ? outActive : inActive).at(index) = active; return 0;
    }
    SBResult PLUGIN_API setActive(Steinberg::TBool) override { return 0; }
    SBResult PLUGIN_API setState(Steinberg::IBStream*) override { return 0; }
    SBResult PLUGIN_API getState(Steinberg::IBStream*) override { return 0; }
    SBResult PLUGIN_API setBusArrangements(Arrangement* inputs, Steinberg::int32 numIns, Arrangement* outputs, Steinberg::int32 numOuts) override {
        nativeRequest = numIns == (int)ins.size() && numOuts == (int)outs.size();
        for (int i = 0; i < numIns; ++i) nativeRequest &= inputs[i] == inArr[i];
        for (int i = 0; i < numOuts; ++i) nativeRequest &= outputs[i] == outArr[i];
        if (adaptRequest) { outs[0].channelCount = 1; outArr[0] = arrangement(1); return Steinberg::kResultFalse; }
        return nativeRequest ? Steinberg::kResultTrue : Steinberg::kResultFalse;
    }
    SBResult PLUGIN_API getBusArrangement(Steinberg::Vst::BusDirection dir, Steinberg::int32 index, Arrangement& arr) override {
        arr = (dir ? outArr : inArr).at(index); return 0;
    }
    SBResult PLUGIN_API canProcessSampleSize(Steinberg::int32) override { return 0; }
    Steinberg::uint32 PLUGIN_API getLatencySamples() override { return 0; }
    SBResult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) override { setup = true; return 0; }
    SBResult PLUGIN_API setProcessing(Steinberg::TBool) override { return 0; }
    SBResult PLUGIN_API process(Steinberg::Vst::ProcessData&) override { return 0; }
    Steinberg::uint32 PLUGIN_API getTailSamples() override { return 0; }
};
}

#ifndef VAMP_FACTORY_FIXTURE
static void pluginBusTests() {
    namespace V = Steinberg::Vst;
    PluginAudioBuses buses;
    BusTestPlugin plugin;
    V::ProcessSetup setup{V::kRealtime, V::kSample32, MaxBlock, 48000};
    std::array<float, 32> l{}, r{}, sl{}, sr{};
    l.fill(.2f); r.fill(.6f); sl.fill(.4f); sr.fill(.8f);

    plugin.layouts({}, std::vector<BusTestPlugin::Info>(64, BusTestPlugin::bus(2)));
    buses.configure(plugin, plugin); plugin.setupProcessing(setup); buses.activate(plugin);
    check(buses.outputs.size() == 64 && plugin.nativeRequest, "64 native VST3 outputs accepted without forced stereo arrangements");
    check(plugin.outActive[0] && std::count(plugin.outActive.begin(), plugin.outActive.end(), true) == 1 && !plugin.activationBeforeSetup,
          "only the main instrument output is activated, after setupProcessing");
    buses.beginBlock(l.data(), r.data(), 32, nullptr, nullptr);
    auto* allocation = buses.outputs[0].channelBuffers32[0];
    for (int b = 0; b < 64; ++b) {
        check(buses.outputs[b].numChannels == 2 && buses.outputs[b].channelBuffers32[1] != nullptr, "inactive output bus indices and buffers retained");
        if (b) check(buses.outputs[b].channelBuffers32[0] != allocation, "auxiliary outputs never alias the main output");
    }
    std::fill_n(buses.outputs[0].channelBuffers32[0], 32, .1f);
    std::fill_n(buses.outputs[0].channelBuffers32[1], 32, -.3f);
    std::fill_n(buses.outputs[63].channelBuffers32[0], 32, .9f);
    buses.copyOutput(l.data(), r.data(), 32);
    check(l[0] == .1f && r[0] == -.3f, "additional instrument outputs are not duplicated into the stereo mix");
    buses.beginBlock(l.data(), r.data(), 32, nullptr, nullptr);
    check(buses.outputs[0].channelBuffers32[0] == allocation && allocation[0] == 0 && buses.outputs[63].channelBuffers32[0][0] == .9f,
          "audio block keeps buffers and does not clear inactive outputs");

    plugin.layouts({BusTestPlugin::bus(1)}, {BusTestPlugin::bus(1), BusTestPlugin::bus(6, V::kAux)});
    buses.configure(plugin, plugin);
    check(plugin.nativeRequest && buses.outputs[1].numChannels == 6, "mono and multichannel auxiliary arrangements preserved");
    l.fill(.2f); r.fill(.6f); buses.beginBlock(l.data(), r.data(), 32, nullptr, nullptr);
    check(std::abs(buses.inputs[0].channelBuffers32[0][0] - .4f) < 1e-6, "stereo host input downmixed for a mono bus");
    std::fill_n(buses.outputs[0].channelBuffers32[0], 32, .7f); buses.copyOutput(l.data(), r.data(), 32);
    check(l[0] == .7f && r[0] == .7f, "mono output feeds both stereo channels");

    plugin.layouts({BusTestPlugin::bus(2), BusTestPlugin::bus(2), BusTestPlugin::bus(1, V::kAux)}, {BusTestPlugin::bus(0), BusTestPlugin::bus(2)});
    buses.configure(plugin, plugin); plugin.setupProcessing(setup); buses.activate(plugin);
    l.fill(.2f); r.fill(.6f); buses.beginBlock(l.data(), r.data(), 32, sl.data(), sr.data());
    check(buses.mainOutput == 1 && buses.outputs[0].numChannels == 0 && plugin.outActive[1], "empty output buses accepted and usable main output selected");
    check(buses.sideInput == 2 && !plugin.inActive[1] && plugin.inActive[2] && buses.inputs[1].silenceFlags == 3,
          "sidechain uses the auxiliary bus, not a second main bus");
    check(std::abs(buses.inputs[2].channelBuffers32[0][0] - .6f) < 1e-6, "mono sidechain receives stereo downmix");
    buses.beginBlock(l.data(), r.data(), 32, nullptr, nullptr);
    check(buses.inputs[2].silenceFlags == 1 && buses.inputs[2].channelBuffers32[0][0] == 0, "disconnected sidechain is silent");

    plugin.layouts({}, {BusTestPlugin::bus(2)}); plugin.adaptRequest = true;
    buses.configure(plugin, plugin);
    check(buses.outputs[0].numChannels == 1, "layout read back after setBusArrangements returns false");
    plugin.adaptRequest = false;
    plugin.layouts({}, std::vector<BusTestPlugin::Info>(32, BusTestPlugin::bus(2)));
    buses.configure(plugin, plugin);
    check(buses.outputs.size() == 32 && buses.outputs[0].numChannels == 2, "buffers rebuilt when preset or sample rate changes layout");
    auto cv = BusTestPlugin::bus(2); cv.flags = V::BusInfo::kIsControlVoltage;
    plugin.layouts({BusTestPlugin::bus(64, V::kAux)}, {cv, BusTestPlugin::bus(2)});
    buses.configure(plugin, plugin); buses.beginBlock(l.data(), r.data(), 32, nullptr, nullptr);
    check(buses.mainOutput == 1 && buses.inputs[0].channelBuffers32[63] != nullptr, "CV outputs excluded and 64-channel pointer arrays are complete");
    bool rejected = false;
    plugin.layouts({}, std::vector<BusTestPlugin::Info>(257, BusTestPlugin::bus(2)));
    try { buses.configure(plugin, plugin); } catch (const std::exception&) { rejected = true; }
    check(rejected, "malformed bus counts rejected before allocation");
    rejected = false;
    plugin.layouts({}, std::vector<BusTestPlugin::Info>(33, BusTestPlugin::bus(64)));
    try { buses.configure(plugin, plugin); } catch (const std::exception&) { rejected = true; }
    check(rejected, "excessive channel counts bounded before allocation");
    std::ofstream(L"work/test-results/vst-buses.json") << Json({{"passed",true},{"large_output_buses",64},
        {"native_mono_and_aux",true},{"empty_bus",true},{"sidechain_mapping",true},{"layout_readback",true},
        {"layout_reconfiguration",true},{"inactive_output_clearing_skipped",true},{"malformed_counts_rejected",true}}).dump(2);
}
#endif
