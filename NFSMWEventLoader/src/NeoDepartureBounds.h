#pragma once
#include <cmath>
#include <algorithm>

namespace neo_departure {
// Pure one-dimensional propagation in consistent units (m, s, m/s, m/s^2).
// The caller supplies acceleration bounds valid over the entire approach.
// Historical observations alone do not establish that premise. No vehicle,
// event, controller, diagnostics, speed-cap or physics mutation is involved.
struct Arrival {
    double minimumSpeed = 0, maximumSpeed = 0;
    double earliestSeconds = 0, latestSeconds = 0;
};
inline bool Endpoint(double speed, double distance, double acceleration,
                     double& endSpeed, double& seconds) {
    const double square = speed * speed;
    if (!std::isfinite(square)) return false;
    const double energy = std::fma(2.0 * acceleration, distance, square);
    // Zero energy means no positive-speed departure is certified. This also
    // rejects a possible stop before the takeoff point under the lower bound.
    if (!std::isfinite(energy) || energy <= 0) return false;
    endSpeed = std::sqrt(energy);
    const double meanSpeed = (speed + endSpeed) * .5;
    // Stable even for acceleration close to zero: do not divide by a or
    // subtract two nearly equal velocities to obtain the arrival time.
    seconds = distance / meanSpeed;
    return std::isfinite(seconds) && seconds >= 0;
}
inline bool Propagate(double minimumInitialSpeed, double maximumInitialSpeed,
                      double distance, double minimumAcceleration,
                      double maximumAcceleration, Arrival& output) {
    output = {};
    if (!std::isfinite(minimumInitialSpeed) || !std::isfinite(maximumInitialSpeed) ||
        !std::isfinite(distance) || !std::isfinite(minimumAcceleration) ||
        !std::isfinite(maximumAcceleration) || minimumInitialSpeed < 0 ||
        maximumInitialSpeed < minimumInitialSpeed || distance < 0 ||
        maximumAcceleration < minimumAcceleration) return false;
    if (distance == 0) {
        output.minimumSpeed = minimumInitialSpeed;
        output.maximumSpeed = maximumInitialSpeed;
        return true;
    }
    Arrival result;
    if (!Endpoint(minimumInitialSpeed, distance, minimumAcceleration,
                  result.minimumSpeed, result.latestSeconds) ||
        !Endpoint(maximumInitialSpeed, distance, maximumAcceleration,
                  result.maximumSpeed, result.earliestSeconds)) return false;
    if (result.minimumSpeed > result.maximumSpeed ||
        result.earliestSeconds > result.latestSeconds) return false;
    output = result;
    return true;
}
inline bool Propagate(double speed, double distance, double minimumAcceleration,
                      double maximumAcceleration, Arrival& output) {
    return Propagate(speed, speed, distance, minimumAcceleration, maximumAcceleration, output);
}
} // namespace neo_departure

