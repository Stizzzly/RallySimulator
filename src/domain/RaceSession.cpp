#include "RaceSession.h"
#include <algorithm>
#include <cmath>

namespace rally {
namespace { constexpr float kPi = 3.14159265f; }

void RaceSession::start(const StageConfig& stage, const CarState& startingCar) {
    stage_ = stage; car_ = startingCar; elapsed_ = 0.0f; checkpoint_ = 0; finished_ = false;
}

void RaceSession::update(float dt, const DriveInput& input, SurfaceType surface, float groundY) {
    if (finished_) return;
    dt = std::min(dt, 0.05f);
    elapsed_ += dt;
    const float acceleration = surface == SurfaceType::Asphalt ? 35.0f : surface == SurfaceType::Gravel ? 28.0f : 14.0f;
    const float friction = surface == SurfaceType::Asphalt ? 1.5f : surface == SurfaceType::Gravel ? 2.2f : 5.0f;
    float grip = (surface == SurfaceType::Asphalt ? 6.0f : surface == SurfaceType::Gravel ? 2.4f : 1.0f) * dt;
    car_.speed += input.throttle * acceleration * dt;
    car_.speed -= input.brake * acceleration * .6f * dt;
    if (input.throttle == 0.0f && input.brake == 0.0f) {
        car_.speed -= car_.speed * friction * dt;
        if (std::fabs(car_.speed) < 0.1f) car_.speed = 0.0f;
    }
    car_.speed = std::max(-27.5f, std::min(55.0f, car_.speed));
    if (std::fabs(car_.speed) > .5f) car_.heading -= input.steering * 100.0f * dt * (car_.speed > 0 ? 1.0f : -1.0f);
    const float rad = car_.heading * kPi / 180.0f;
    const float targetX = -std::sin(rad) * car_.speed;
    const float targetZ = -std::cos(rad) * car_.speed;

    // Вертикальная динамика: гравитация, приземление, потеря сцепления в полёте.
    car_.velocityY -= 40.0f * dt;
    car_.y += car_.velocityY * dt;
    if (car_.y <= groundY) {
        car_.y = groundY;
        car_.velocityY = 0.0f;
    } else {
        grip *= 0.1f; // в полёте руль почти не работает
    }

    car_.velocityX += (targetX - car_.velocityX) * grip;
    car_.velocityZ += (targetZ - car_.velocityZ) * grip;
    car_.x += car_.velocityX * dt; car_.z += car_.velocityZ * dt;
}

bool RaceSession::updateProgress(float distance) {
    if (finished_) return false;
    const int nextDistance = (checkpoint_ + 1) * stage_.checkpointInterval;
    if (distance < nextDistance && distance < stage_.length) return false;
    ++checkpoint_;
    if (distance >= stage_.length) finished_ = true;
    return true;
}

int RaceSession::checkpointCount() const { return (stage_.length + stage_.checkpointInterval - 1) / stage_.checkpointInterval; }
Medal RaceSession::medalFor(const StageConfig& s, float t) { if (t <= s.goldTime) return Medal::Gold; if (t <= s.silverTime) return Medal::Silver; if (t <= s.bronzeTime) return Medal::Bronze; return Medal::None; }
RaceResult RaceSession::finish(float best) const { RaceResult r; r.elapsed = elapsed_; r.medal = medalFor(stage_, elapsed_); r.newBest = best <= 0.0f || elapsed_ < best; return r; }

} // namespace rally
