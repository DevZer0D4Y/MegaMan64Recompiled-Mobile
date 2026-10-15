#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { MM_VR_ABI = 1, MM_VR_ORIENTATION_VALID = 1, MM_VR_POSITION_VALID = 2 };
typedef struct MMVrPose {
    float orientation[4]; /* quaternion x, y, z, w */
    float position[3];    /* OpenXR LOCAL space, metres, right-handed, -Z forward */
    uint32_t flags;
} MMVrPose;
typedef struct MMVrEye {
    MMVrPose pose;
    float fovLeft, fovRight, fovUp, fovDown; /* radians */
} MMVrEye;
typedef struct MMVrTracking {
    uint32_t abi;
    uint32_t size;
    uint64_t sequence;
    int64_t predictedDisplayTime;
    uint32_t focused;
    uint32_t eyeCount;
    uint32_t originGeneration;
    uint32_t reserved;
    MMVrPose head;
    MMVrEye eyes[2];
    MMVrPose grip[2];
    MMVrPose aim[2];
    float trigger[2];
    float squeeze[2];
    float stick[2][2];
} MMVrTracking;

/* Copies snapshots; the OpenXR producer and game/render consumers never share mutable poses. */
void mm_vr_submit_tracking(const MMVrTracking* tracking);
int mm_vr_read_tracking(MMVrTracking* output, uint32_t outputSize);
void mm_vr_clear_tracking(void);
/* source 0: Android resumed; source 1: OpenXR focused. Both must be active. */
void mm_vr_set_tracking_status(uint32_t source, int active);

#ifdef __cplusplus
}
static_assert(sizeof(MMVrPose) == 32 && sizeof(MMVrEye) == 48 && sizeof(MMVrTracking) == 328, "VR tracking ABI changed");
#endif
