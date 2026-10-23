#pragma once
#include "model.hpp"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include <cstring>
#include <stdexcept>

namespace vamp {
// Bus indices must stay identical to the component's, including inactive buses.
// Allocate buffers only while stopped; beginBlock/copyOutput never allocate.
class PluginAudioBuses {
    using BusInfo = Steinberg::Vst::BusInfo;
    using Arrangement = Steinberg::Vst::SpeakerArrangement;
    std::vector<BusInfo> inputInfo, outputInfo;
    std::vector<std::vector<float*>> inputPointers, outputPointers;
    std::vector<float> outputSamples, zero, mono, sideMono;
    static constexpr int MaxBuses = 256, MaxChannels = 64, MaxTotalChannels = 2048;

    static std::vector<BusInfo> describe(Steinberg::Vst::IComponent& component, int direction) {
        namespace V = Steinberg::Vst;
        int count = component.getBusCount(V::kAudio, direction);
        if (count < 0 || count > MaxBuses)
            throw std::runtime_error("VST3 reports an invalid audio bus count: " + std::to_string(count));
        std::vector<BusInfo> result(count);
        int channels = 0;
        for (int b = 0; b < count; ++b) {
            if (component.getBusInfo(V::kAudio, direction, b, result[b]) != Steinberg::kResultOk)
                throw std::runtime_error("Cannot read VST3 audio bus " + std::to_string(b));
            auto n = result[b].channelCount;
            if (n < 0 || n > MaxChannels || (channels += n) > MaxTotalChannels)
                throw std::runtime_error("VST3 reports an invalid audio channel count");
        }
        return result;
    }
    static bool audible(const BusInfo& bus) {
        return bus.channelCount > 0 && !(bus.flags & BusInfo::kIsControlVoltage);
    }
    static int mainBus(const std::vector<BusInfo>& buses) {
        for (int b = 0; b < (int)buses.size(); ++b)
            if (audible(buses[b]) && buses[b].busType == Steinberg::Vst::kMain) return b;
        for (int b = 0; b < (int)buses.size(); ++b) if (audible(buses[b])) return b;
        return -1;
    }
    static Steinberg::uint64 silenceMask(int channels) {
        return channels == 64 ? ~Steinberg::uint64(0) : (Steinberg::uint64(1) << channels) - 1;
    }
public:
    std::vector<Steinberg::Vst::AudioBusBuffers> inputs, outputs;
    int mainInput = -1, sideInput = -1, mainOutput = -1;

    void configure(Steinberg::Vst::IComponent& component, Steinberg::Vst::IAudioProcessor& processor) {
        namespace V = Steinberg::Vst;
        inputInfo = describe(component, V::kInput);
        outputInfo = describe(component, V::kOutput);
        std::vector<Arrangement> ins(inputInfo.size()), outs(outputInfo.size());
        bool known = true;
        for (int b = 0; b < (int)ins.size(); ++b)
            known &= processor.getBusArrangement(V::kInput, b, ins[b]) == Steinberg::kResultOk;
        for (int b = 0; b < (int)outs.size(); ++b)
            known &= processor.getBusArrangement(V::kOutput, b, outs[b]) == Steinberg::kResultOk;
        // Preserve mono, empty and auxiliary arrangements instead of forcing stereo.
        // A rejected request may still change the layout (VST3 contract): always read it back.
        if (known) processor.setBusArrangements(ins.empty() ? nullptr : ins.data(), (int)ins.size(),
                                              outs.empty() ? nullptr : outs.data(), (int)outs.size());
        inputInfo = describe(component, V::kInput);
        outputInfo = describe(component, V::kOutput);
        mainInput = mainBus(inputInfo);
        mainOutput = mainBus(outputInfo);
        if (mainOutput < 0) throw std::runtime_error("VST3 has no usable audio output");
        sideInput = -1;
        for (int b = 0; b < (int)inputInfo.size(); ++b)
            if (b != mainInput && audible(inputInfo[b]) && inputInfo[b].busType == V::kAux) {
                sideInput = b; break;
            }
        inputs.assign(inputInfo.size(), {});
        outputs.assign(outputInfo.size(), {});
        inputPointers.resize(inputs.size());
        outputPointers.resize(outputs.size());
        size_t outputChannels = 0;
        for (auto& bus : outputInfo) outputChannels += bus.channelCount;
        outputSamples.assign(outputChannels * MaxBlock, 0.f);
        zero.assign(MaxBlock, 0.f);
        mono.resize(mainInput >= 0 && inputInfo[mainInput].channelCount == 1 ? MaxBlock : 0);
        sideMono.resize(sideInput >= 0 && inputInfo[sideInput].channelCount == 1 ? MaxBlock : 0);
        for (int b = 0; b < (int)inputs.size(); ++b) {
            inputs[b].numChannels = inputInfo[b].channelCount;
            inputPointers[b].assign(inputs[b].numChannels, zero.data());
            inputs[b].channelBuffers32 = inputPointers[b].data();
        }
        size_t offset = 0;
        for (int b = 0; b < (int)outputs.size(); ++b) {
            outputs[b].numChannels = outputInfo[b].channelCount;
            outputPointers[b].resize(outputs[b].numChannels);
            for (auto& pointer : outputPointers[b]) { pointer = outputSamples.data() + offset; offset += MaxBlock; }
            outputs[b].channelBuffers32 = outputPointers[b].data();
        }
    }
    void activate(Steinberg::Vst::IComponent& component) {
        namespace V = Steinberg::Vst;
        for (int b = 0; b < (int)inputs.size(); ++b)
            component.activateBus(V::kAudio, V::kInput, b, b == mainInput || b == sideInput);
        for (int b = 0; b < (int)outputs.size(); ++b)
            component.activateBus(V::kAudio, V::kOutput, b, b == mainOutput);
    }
    void beginBlock(float* left, float* right, int frames, const float* sideLeft, const float* sideRight) {
        for (int b = 0; b < (int)inputs.size(); ++b) {
            auto& bus = inputs[b];
            std::fill(inputPointers[b].begin(), inputPointers[b].end(), zero.data());
            bus.silenceFlags = silenceMask(bus.numChannels);
            const float* l = b == mainInput ? left : b == sideInput ? sideLeft : nullptr;
            const float* r = b == mainInput ? right : b == sideInput ? sideRight : nullptr;
            if (!l || bus.numChannels == 0) continue;
            if (!r) r = l;
            if (bus.numChannels == 1) {
                auto& scratch = b == mainInput ? mono : sideMono;
                for (int i = 0; i < frames; ++i) scratch[i] = (l[i] + r[i]) * .5f;
                inputPointers[b][0] = scratch.data();
                bus.silenceFlags = 0;
            } else {
                inputPointers[b][0] = const_cast<float*>(l);
                inputPointers[b][1] = const_cast<float*>(r);
                bus.silenceFlags &= ~Steinberg::uint64(3);
            }
        }
        for (int b = 0; b < (int)outputs.size(); ++b) {
            outputs[b].silenceFlags = b == mainOutput ? 0 : silenceMask(outputs[b].numChannels);
            // Inactive outputs retain valid distinct buffers, but cost no per-block clearing.
            if (b == mainOutput) for (auto* channel : outputPointers[b]) std::fill_n(channel, frames, 0.f);
        }
    }
    void copyOutput(float* left, float* right, int frames) const {
        auto& bus = outputs[mainOutput];
        std::memcpy(left, bus.channelBuffers32[0], frames * sizeof(float));
        std::memcpy(right, bus.channelBuffers32[bus.numChannels == 1 ? 0 : 1], frames * sizeof(float));
    }
    Json description() const {
        Json result = {{"inputs", Json::array()}, {"outputs", Json::array()},
                       {"main_input", mainInput}, {"sidechain_input", sideInput}, {"main_output", mainOutput}};
        for (auto& bus : inputInfo) result["inputs"].push_back(bus.channelCount);
        for (auto& bus : outputInfo) result["outputs"].push_back(bus.channelCount);
        return result;
    }
};
}
