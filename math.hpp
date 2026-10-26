#pragma once
#include <cmath>
#include <array>

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
using Mat4 = std::array<float, 16>;

inline Vec2 WorldToScreen(const Vec3& world, const Mat4& vp, float sw, float sh) {
    float cx = vp[0]*world.x + vp[4]*world.y + vp[8]*world.z  + vp[12];
    float cy = vp[1]*world.x + vp[5]*world.y + vp[9]*world.z  + vp[13];
    float cw = vp[3]*world.x + vp[7]*world.y + vp[11]*world.z + vp[15];
    if (cw <= 0.001f) return {-1,-1};
    return {
        (cx/cw + 1.f) * 0.5f * sw,
        (1.f - cy/cw) * 0.5f * sh
    };
}

inline float Distance2D(Vec2 a, Vec2 b) {
    return sqrtf((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}
