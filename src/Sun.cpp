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
