#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace shadow::math {

using Mat4 = std::array<float, 16>;

inline Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) sum += double(a[static_cast<std::size_t>(k * 4 + row)]) * double(b[static_cast<std::size_t>(c * 4 + k)]);
            r[static_cast<std::size_t>(c * 4 + row)] = static_cast<float>(sum);
        }
    }
    return r;
}

inline void mulColumn3(const Mat4& a, const Mat4& b, float out[4]) {
    for (std::size_t row = 0; row < 4; ++row) {
        double sum = 0.0;
        for (std::size_t k = 0; k < 4; ++k) sum += double(a[k * 4 + row]) * double(b[12 + k]);
        out[row] = static_cast<float>(sum);
    }
}

inline bool invert(const Mat4& in, Mat4& out) {
    double m[16], inv[16];
    for (std::size_t i = 0; i < 16; ++i) m[i] = double(in[i]);
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (!(std::fabs(det) > 1e-30)) return false;
    for (std::size_t i = 0; i < 16; ++i) out[i] = static_cast<float>(inv[i] / det);
    return true;
}

inline bool transformPoint(const Mat4& m, float x, float y, float z, float out[3]) {
    const double w = double(m[3]) * x + double(m[7]) * y + double(m[11]) * z + double(m[15]);
    if (!(std::fabs(w) > 1e-12)) return false;
    for (int r = 0; r < 3; ++r) {
        const double v = double(m[static_cast<std::size_t>(r)]) * x + double(m[static_cast<std::size_t>(4 + r)]) * y +
                         double(m[static_cast<std::size_t>(8 + r)]) * z + double(m[static_cast<std::size_t>(12 + r)]);
        out[r] = static_cast<float>(v / w);
        if (!std::isfinite(out[r])) return false;
    }
    return true;
}

inline bool plausibleProj(const Mat4& p) { return std::fabs(p[11] + 1.0f) < 0.01f && std::fabs(p[15]) < 0.01f && p[0] > 0.05f && std::fabs(p[5]) > 0.05f; }
inline bool plausibleView(const Mat4& v) {
    for (std::size_t col = 0; col < 3; ++col) {
        const float x = v[col * 4], y = v[col * 4 + 1], z = v[col * 4 + 2];
        const float len = std::sqrt(x * x + y * y + z * z);
        if (len < 0.9f || len > 1.1f) return false;
    }
    return true;
}
inline bool plausibleVP(const Mat4& m) {
    const float len = std::sqrt(m[3] * m[3] + m[7] * m[7] + m[11] * m[11]);
    return len > 0.9f && len < 1.1f;
}

struct Light {
    Mat4 view{}, proj{}, vp{};
    float texel = 0.0f, range = 1.0f;
};

inline void buildLight(const float centerIn[3], const float sun[3], float radius, int res, Light& out) {
    double z[3] = {sun[0], sun[1], sun[2]};
    double zl = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    for (double& v : z) v /= zl;
    double up[3] = {0.0, 1.0, 0.0};
    if (std::fabs(z[1]) > 0.99) { up[0] = 1.0; up[1] = 0.0; }
    double x[3] = {up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]};
    const double xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    for (double& v : x) v /= xl;
    const double y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};

    double c[3] = {centerIn[0], centerIn[1], centerIn[2]};
    const double texel = 2.0 * double(radius) / double(res);
    const double cx = x[0] * c[0] + x[1] * c[1] + x[2] * c[2], cy = y[0] * c[0] + y[1] * c[1] + y[2] * c[2];
    const double sx = std::floor(cx / texel + 0.5) * texel, sy = std::floor(cy / texel + 0.5) * texel;
    for (int i = 0; i < 3; ++i) c[i] += x[i] * (sx - cx) + y[i] * (sy - cy);

    const double dz = double(radius) * 1.5 + 64.0;
    const double range = 2.0 * dz;
    double eye[3];
    for (int i = 0; i < 3; ++i) eye[i] = c[i] + z[i] * dz;

    Mat4 v{};
    v[0] = float(x[0]); v[1] = float(y[0]); v[2] = float(z[0]);
    v[4] = float(x[1]); v[5] = float(y[1]); v[6] = float(z[1]);
    v[8] = float(x[2]); v[9] = float(y[2]); v[10] = float(z[2]);
    v[12] = float(-(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]));
    v[13] = float(-(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]));
    v[14] = float(-(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]));
    v[15] = 1.0f;

    Mat4 p{};
    p[0] = float(1.0 / radius);
    p[5] = float(1.0 / radius);
    p[10] = float(-2.0 / range);
    p[14] = -1.0f;
    p[15] = 1.0f;

    out.view = v;
    out.proj = p;
    out.vp = mul(p, v);
    out.texel = float(texel);
    out.range = float(range);
}

}
