#pragma once
#include "tracking.h"
#include <cmath>

namespace immersive {
struct Vec3 { float x, y, z; };
struct Matrix { float value[16]; }; // row-major storage, column-vector convention
inline Vec3 rotate(const MMVrPose& pose, Vec3 v) {
    const auto* q = pose.orientation;
    Vec3 t{2 * (q[1] * v.z - q[2] * v.y), 2 * (q[2] * v.x - q[0] * v.z), 2 * (q[0] * v.y - q[1] * v.x)};
    return {v.x + q[3] * t.x + q[1] * t.z - q[2] * t.y,
            v.y + q[3] * t.y + q[2] * t.x - q[0] * t.z,
            v.z + q[3] * t.z + q[0] * t.y - q[1] * t.x};
}
inline bool aimRay(const MMVrPose& aim, Vec3& origin, Vec3& direction) {
    if ((aim.flags & 3) != 3) return false;
    origin = {aim.position[0], aim.position[1], aim.position[2]};
    direction = rotate(aim, {0, 0, -1});
    return true;
}
inline Matrix view(const MMVrPose& pose) {
    MMVrPose inverse = pose;
    for (int i = 0; i < 3; ++i) inverse.orientation[i] *= -1;
    const Vec3 x = rotate(inverse, {1, 0, 0});
    const Vec3 y = rotate(inverse, {0, 1, 0});
    const Vec3 z = rotate(inverse, {0, 0, 1});
    const Vec3 p = rotate(inverse, {-pose.position[0], -pose.position[1], -pose.position[2]});
    return {{x.x, y.x, z.x, p.x, x.y, y.y, z.y, p.y, x.z, y.z, z.z, p.z, 0, 0, 0, 1}};
}
/* Vulkan depth range [0,1], right-handed -Z forward; no assumed symmetric FOV/IPD. */
inline bool projection(const MMVrEye& eye, float nearPlane, float farPlane, Matrix& result) {
    if (!(nearPlane > 0 && farPlane > nearPlane) || !std::isfinite(farPlane)) return false;
    if (std::abs(eye.fovLeft) >= 1.56f || std::abs(eye.fovRight) >= 1.56f || std::abs(eye.fovUp) >= 1.56f || std::abs(eye.fovDown) >= 1.56f) return false;
    const float left = std::tan(eye.fovLeft), right = std::tan(eye.fovRight);
    const float down = std::tan(eye.fovDown), up = std::tan(eye.fovUp);
    if (!(right > left && up > down) || !std::isfinite(right - left) || !std::isfinite(up - down)) return false;
    const float width = right - left, height = up - down;
    result = {{2 / width, 0, (right + left) / width, 0,
               0, -2 / height, -(up + down) / height, 0,
               0, 0, -farPlane / (farPlane - nearPlane), -nearPlane * (farPlane / (farPlane - nearPlane)),
               0, 0, -1, 0}};
    return true;
}
}
