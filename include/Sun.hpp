#pragma once

namespace shadow {

struct SunParams {
    bool dayCycle = true;
    int timeOfDay = -1;
    float azimuth = 90.0f;
    float elevation = 55.0f;
    float tilt = 0.0f;
    bool moon = true;
};

struct SunResult {
    float dir[3] = {0.0f, 1.0f, 0.0f};
    float strength = 1.0f;
    bool night = false;
    bool fromTime = false;
};

float celestialAngle(int ticks);

SunResult computeSun(const SunParams& p);

// Rotation that carries the game's own sun and moon in the sky onto the light direction computeSun() uses, as (R - I) in column-major mat3
// order (all zero = leave the sky alone). Day cycle on: tilt + compass yaw, independent of the time. Day cycle off: the fixed compass sun
// (needs p.timeOfDay to know where the game puts its sun). The moon sits opposite the sun, so the same rotation moves it too.
// Returns false (and zeros) when nothing has to move.
bool computeSunDelta(const SunParams& p, float delta[9]);

}
