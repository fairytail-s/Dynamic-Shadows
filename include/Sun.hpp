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

}
