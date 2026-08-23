#include "TrackModel.h"
#include <cmath>

namespace rally {
namespace {

float gravelNoise(float z) { return std::sin(z * 0.015f) + std::cos(z * 0.045f); }
} // namespace

TrackModel::TrackModel(std::string theme) : theme_(std::move(theme)) {}

float TrackModel::centerX(float z) const {
    if (theme_ == "forest") return std::sin(z * .022f) * 28.0f + std::sin(z * .057f) * 14.0f;
    if (theme_ == "coast") return std::sin(z * .010f) * 55.0f + std::sin(z * .031f) * 15.0f;
    // mountain: плавные изгибы + резкие повороты
    return std::sin(z * 0.015f) * 40.0f
         + std::sin(z * 0.04f) * 20.0f
         + std::cos(z * 0.08f) * 10.0f;
}

float TrackModel::heightAt(float x, float z) const {
    const float center = centerX(z);
    const float dist = std::fabs(x - center);
    const float baseHeight = std::sin(z * 0.03f) * 3.0f;

    if (dist < kRoadWidth + kCurbWidth) return baseHeight + 0.2f; // плоская дорога

    // Равнина/обочина шириной 25 м с лёгкими кочками.
    if (dist < kRoadWidth + kCurbWidth + 25.0f)
        return baseHeight + 0.2f + std::sin(x * 0.4f) * 0.3f;

    // Горы начинаются далеко по краям.
    const float wallDist = dist - (kRoadWidth + kCurbWidth + 25.0f);
    return baseHeight + 0.2f + (wallDist * wallDist * 0.05f) + std::sin(x) * 0.5f;
}

SurfaceType TrackModel::surfaceAt(float x, float z) const {
    if (std::fabs(x - centerX(z)) > kRoadWidth + 0.5f) return SurfaceType::Grass;
    if (gravelNoise(z) > 0.8f) return SurfaceType::Gravel;
    return SurfaceType::Asphalt;
}

CarState TrackModel::startingCar() const {
    CarState car;
    car.x = centerX(0.0f);
    car.y = heightAt(car.x, 0.0f);
    // Старт как в аркадном оригинале: строго вдоль трассы в +Z,
    // дорога сама уводит машину за поворотом.
    car.heading = 180.0f;
    return car;
}

} // namespace rally
