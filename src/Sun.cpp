#include "Sun.hpp"

#include <algorithm>
#include <cmath>

namespace shadow {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr float kMinLightY = 0.17f;
constexpr float kHorizonFade = 0.25f;
constexpr float kMoonStrength = 0.6f;

float smooth(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void fromCompass(float azimuthDeg, float elevationDeg, float out[3]) {
    const double az = double(azimuthDeg) * kDeg, el = double(elevationDeg) * kDeg;
    out[0] = float(std::cos(el) * std::sin(az));
    out[1] = float(std::sin(el));
    out[2] = float(-std::cos(el) * std::cos(az));
}
}

float celestialAngle(int ticks) {
    const int day = ((ticks % 24000) + 24000) % 24000;
    double t = double(day) / 24000.0 - 0.25;
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    const double smoothed = 1.0 - (std::cos(t * kPi) + 1.0) / 2.0;
    return float(t + (smoothed - t) / 3.0);
}

bool computeSunDelta(const SunParams& p, float delta[9]) {
    for (int i = 0; i < 9; ++i) delta[i] = 0.0f;
    double r[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    if (p.dayCycle && p.timeOfDay >= 0) {
        // Same two rotations computeSun() applies to the sun path: tilt about X, then yaw about Y.
        const double tilt = double(p.tilt) * kDeg, yaw = (double(p.azimuth) - 90.0) * kDeg;
        const double ct = std::cos(tilt), st = std::sin(tilt), cy = std::cos(yaw), sy = std::sin(yaw);
        const double rt[3][3] = {{1, 0, 0}, {0, ct, -st}, {0, st, ct}};
        const double ry[3][3] = {{cy, 0, -sy}, {0, 1, 0}, {sy, 0, cy}};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r[i][j] = ry[i][0] * rt[0][j] + ry[i][1] * rt[1][j] + ry[i][2] * rt[2][j];
    } else if (!p.dayCycle && p.timeOfDay >= 0) {
        // Fixed sun: smallest rotation from where the game draws the sun now to the chosen compass position.
        const double theta = double(celestialAngle(p.timeOfDay)) * 2.0 * kPi;
        const double g[3] = {-std::sin(theta), std::cos(theta), 0.0};
        float tf[3];
        fromCompass(p.azimuth, p.elevation, tf);
        const double t[3] = {tf[0], tf[1], tf[2]};
        const double v[3] = {g[1] * t[2] - g[2] * t[1], g[2] * t[0] - g[0] * t[2], g[0] * t[1] - g[1] * t[0]};
        const double c = g[0] * t[0] + g[1] * t[1] + g[2] * t[2];
        if (c < -0.999999) {
            // Opposite directions: half turn about any axis perpendicular to g (R = 2aa^T - I).
            double a[3] = {0.0, 0.0, 1.0};  // g lies in the XY plane, so Z is perpendicular to it
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) r[i][j] = 2.0 * a[i] * a[j] - (i == j ? 1.0 : 0.0);
        } else {
            const double k[3][3] = {{0, -v[2], v[1]}, {v[2], 0, -v[0]}, {-v[1], v[0], 0}};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    double k2 = 0.0;
                    for (int m = 0; m < 3; ++m) k2 += k[i][m] * k[m][j];
                    r[i][j] = (i == j ? 1.0 : 0.0) + k[i][j] + k2 / (1.0 + c);
                }
        }
    } else {
        return false;
    }
    double biggest = 0.0;
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) {
            const double d = r[row][col] - (row == col ? 1.0 : 0.0);
            delta[col * 3 + row] = float(d);
            biggest = std::max(biggest, std::fabs(d));
        }
    if (biggest < 1e-6) {
        for (int i = 0; i < 9; ++i) delta[i] = 0.0f;
        return false;
    }
    return true;
}

SunResult computeSun(const SunParams& p) {
    SunResult r;
    if (!p.dayCycle || p.timeOfDay < 0) {
        fromCompass(p.azimuth, p.elevation, r.dir);
        return r;
    }

    const double theta = double(celestialAngle(p.timeOfDay)) * 2.0 * kPi;
    double x = -std::sin(theta), y = std::cos(theta), z = 0.0;

    const double tilt = double(p.tilt) * kDeg;
    const double ty = y * std::cos(tilt) - z * std::sin(tilt);
    const double tz = y * std::sin(tilt) + z * std::cos(tilt);
    y = ty;
    z = tz;

    const double yaw = (double(p.azimuth) - 90.0) * kDeg;
    const double rx = x * std::cos(yaw) - z * std::sin(yaw);
    const double rz = z * std::cos(yaw) + x * std::sin(yaw);
    x = rx;
    z = rz;

    r.night = y < 0.0;
    r.fromTime = true;
    const float height = float(std::fabs(y));
    const double sign = r.night ? -1.0 : 1.0;
    double lx = x * sign, ly = y * sign, lz = z * sign;

    if (ly < double(kMinLightY)) {
        const double horizontal = std::sqrt(lx * lx + lz * lz);
        const double scale = horizontal > 1e-9 ? std::sqrt(1.0 - double(kMinLightY) * double(kMinLightY)) / horizontal : 0.0;
        lx *= scale;
        lz *= scale;
        ly = double(kMinLightY);
    }
    r.dir[0] = float(lx);
    r.dir[1] = float(ly);
    r.dir[2] = float(lz);

    r.strength = smooth(0.0f, kHorizonFade, height);
    if (r.night) r.strength = p.moon ? r.strength * kMoonStrength : 0.0f;
    return r;
}
}
