// VecMath.h - Vector / matrix math and world-to-screen projection.
//
// Conventions (also covered by tests/test_math.cpp):
//   * Row-vector convention: clip = [x y z 1] * M, M stored as m[row][col].
//   * World is Z-up. worldBoxToScreen() builds the box by adding to Z.
//   * Screen origin is top-left, +Y down.
#pragma once
#include <cmath>
#include <cstdint>

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
    float length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
    }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float distanceTo(const Vec3& o) const { return (*this - o).length(); }
    Vec3 normalized() const {
        const float l = length();
        return l > 1e-8f ? Vec3{ x / l, y / l, z / l } : Vec3{};
    }
};

struct Matrix4 { float m[4][4] = {}; };

// Projects a world point to screen pixels. Returns false when the point is
// behind the camera (or the matrix produced a non-finite result).
inline bool worldToScreen(const Vec3& world, const Matrix4& vp,
                          float screenW, float screenH, Vec2& out) {
    const float clipX = world.x * vp.m[0][0] + world.y * vp.m[1][0] +
                        world.z * vp.m[2][0] + vp.m[3][0];
    const float clipY = world.x * vp.m[0][1] + world.y * vp.m[1][1] +
                        world.z * vp.m[2][1] + vp.m[3][1];
    const float clipW = world.x * vp.m[0][3] + world.y * vp.m[1][3] +
                        world.z * vp.m[2][3] + vp.m[3][3];
    if (!(clipW >= 0.01f)) return false;   // also rejects NaN
    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) return false;
    out.x = screenW * 0.5f + ndcX * screenW * 0.5f;
    out.y = screenH * 0.5f - ndcY * screenH * 0.5f;
    return true;
}

inline bool worldBoxToScreen(const Vec3& feet, float worldHeight,
                             float worldWidth, const Matrix4& vp,
                             float screenW, float screenH,
                             Vec2& topLeft, Vec2& bottomRight) {
    Vec2 head2d, feet2d;
    const Vec3 head = { feet.x, feet.y, feet.z + worldHeight };
    if (!worldToScreen(feet, vp, screenW, screenH, feet2d)) return false;
    if (!worldToScreen(head, vp, screenW, screenH, head2d)) return false;
    const float height = std::fabs(feet2d.y - head2d.y);
    const float width = height * (worldWidth / worldHeight);
    const float cx = (feet2d.x + head2d.x) * 0.5f;
    topLeft = { cx - width * 0.5f, head2d.y };
    bottomRight = { cx + width * 0.5f, feet2d.y };
    return true;
}

// Builds a view-projection matrix (row-vector convention, Z-up world) for a
// camera at `eye`. yaw = rotation about +Z (0 looks along +X), pitch = up/down.
// Used by Demo Mode only; the real game's matrix is read from its memory.
inline Matrix4 makeViewProjection(const Vec3& eye, float yawRad, float pitchRad,
                                  float fovYRad, float aspect,
                                  float nearZ = 0.1f, float farZ = 2000.0f) {
    const Vec3 f = { std::cos(pitchRad) * std::cos(yawRad),
                     std::cos(pitchRad) * std::sin(yawRad),
                     std::sin(pitchRad) };
    const Vec3 r = f.cross(Vec3{ 0.f, 0.f, 1.f }).normalized();
    const Vec3 u = r.cross(f);

    const float ty = std::tan(fovYRad * 0.5f);
    const float sy = 1.0f / ty;
    const float sx = 1.0f / (ty * aspect);
    const float A  = farZ / (farZ - nearZ);
    const float B  = -nearZ * farZ / (farZ - nearZ);

    Matrix4 M;
    M.m[0][0] = r.x * sx;  M.m[1][0] = r.y * sx;  M.m[2][0] = r.z * sx;  M.m[3][0] = -eye.dot(r) * sx;
    M.m[0][1] = u.x * sy;  M.m[1][1] = u.y * sy;  M.m[2][1] = u.z * sy;  M.m[3][1] = -eye.dot(u) * sy;
    M.m[0][2] = f.x * A;   M.m[1][2] = f.y * A;   M.m[2][2] = f.z * A;   M.m[3][2] = -eye.dot(f) * A + B;
    M.m[0][3] = f.x;       M.m[1][3] = f.y;       M.m[2][3] = f.z;       M.m[3][3] = -eye.dot(f);
    return M;
}
