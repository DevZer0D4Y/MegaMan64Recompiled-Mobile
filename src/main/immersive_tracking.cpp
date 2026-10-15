#include "vr/tracking.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>

namespace {
std::mutex trackingMutex;
MMVrTracking latest{};
std::chrono::steady_clock::time_point received{};
bool valid = false;
uint32_t activeSources = 0;

void sanitize(MMVrPose& pose) {
    pose.flags &= MM_VR_ORIENTATION_VALID | MM_VR_POSITION_VALID;
    float norm = 0;
    for (float q : pose.orientation) norm += q * q;
    if (!std::isfinite(norm) || norm < 1e-8f) {
        pose.flags &= ~MM_VR_ORIENTATION_VALID;
        pose.orientation[0] = pose.orientation[1] = pose.orientation[2] = 0;
        pose.orientation[3] = 1;
    } else {
        const float scale = 1 / std::sqrt(norm);
        for (float& q : pose.orientation) q *= scale;
    }
    for (float position : pose.position) {
        if (!std::isfinite(position)) { pose.flags &= ~MM_VR_POSITION_VALID; break; }
    }
    if (!(pose.flags & MM_VR_POSITION_VALID)) for (float& position : pose.position) position = 0;
}
float axis(float value, float low, float high) {
    return std::isfinite(value) ? std::clamp(value, low, high) : 0;
}
}

extern "C" __attribute__((visibility("default"))) void mm_vr_submit_tracking(const MMVrTracking* input) {
    if (!input || input->abi != MM_VR_ABI || input->size != sizeof(MMVrTracking)) return;
    MMVrTracking copy = *input;
    if (!copy.focused || copy.eyeCount != 2) { mm_vr_clear_tracking(); return; }
    sanitize(copy.head);
    if ((copy.head.flags & 3) != 3) { mm_vr_clear_tracking(); return; }
    for (auto& eye : copy.eyes) {
        sanitize(eye.pose);
        if ((eye.pose.flags & 3) != 3 || !std::isfinite(eye.fovLeft) || !std::isfinite(eye.fovRight) ||
            !std::isfinite(eye.fovUp) || !std::isfinite(eye.fovDown) ||
            eye.fovLeft >= eye.fovRight || eye.fovDown >= eye.fovUp ||
            std::abs(eye.fovLeft) >= 1.56f || std::abs(eye.fovRight) >= 1.56f ||
            std::abs(eye.fovUp) >= 1.56f || std::abs(eye.fovDown) >= 1.56f) { mm_vr_clear_tracking(); return; }
    }
    for (int hand = 0; hand < 2; ++hand) {
        sanitize(copy.grip[hand]); sanitize(copy.aim[hand]);
        copy.trigger[hand] = axis(copy.trigger[hand], 0, 1);
        copy.squeeze[hand] = axis(copy.squeeze[hand], 0, 1);
        for (float& value : copy.stick[hand]) value = axis(value, -1, 1);
        if ((copy.aim[hand].flags & 3) != 3) copy.trigger[hand] = 0;
    }
    const std::scoped_lock lock(trackingMutex);
    if (activeSources != 3 || (valid && copy.sequence <= latest.sequence)) return;
    latest = copy; received = std::chrono::steady_clock::now(); valid = true;
}
extern "C" __attribute__((visibility("default"))) int mm_vr_read_tracking(MMVrTracking* output, uint32_t outputSize) {
    if (!output || outputSize < sizeof(MMVrTracking)) return 0;
    const std::scoped_lock lock(trackingMutex);
    if (!valid || std::chrono::steady_clock::now() - received > std::chrono::milliseconds(250)) {
        *output = {}; return 0;
    }
    *output = latest; return 1;
}
extern "C" __attribute__((visibility("default"))) void mm_vr_clear_tracking() {
    const std::scoped_lock lock(trackingMutex);
    valid = false; latest = {};
}

extern "C" __attribute__((visibility("default"))) void mm_vr_set_tracking_status(uint32_t source, int active) {
    if (source > 1) return;
    const std::scoped_lock lock(trackingMutex);
    if (active) activeSources |= 1u << source;
    else activeSources &= ~(1u << source);
    if (activeSources != 3) { valid = false; latest = {}; }
}
