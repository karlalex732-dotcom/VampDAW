#pragma once
#include "pluginterfaces/base/funknown.h"

namespace vamp {
// Some processors acknowledge this notification with S_FALSE or E_NOTIMPL.
// Component activation and setupProcessing remain mandatory successes; a real
// failure here still aborts activation. Never retry process() during loading.
inline bool processingNotificationAccepted(Steinberg::tresult result) {
    return result==Steinberg::kResultOk || result==Steinberg::kResultFalse || result==Steinberg::kNotImplemented;
}
}
