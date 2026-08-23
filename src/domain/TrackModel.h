#pragma once

#include "RaceTypes.h"

namespace rally {

// Чистая математика трассы: ось дороги, высота рельефа и тип покрытия.
// Никакого SDL/OpenGL/FS — только <cmath>, поэтому тестируется без окна и GPU.
class TrackModel {
public:
    static constexpr float kRoadWidth = 10.0f; // половина ширины дороги
    static constexpr float kCurbWidth = 1.5f;

    explicit TrackModel(std::string theme);

    // Центральная ось дороги на продольной координате z.
    float centerX(float z) const;
    // Высота поверхности в точке (дорога плоская, дальше кочки и горы).
    float heightAt(float x, float z) const;
    // Покрытие под колёсами.
    SurfaceType surfaceAt(float x, float z) const;

    // Стартовая расстановка машины: центр дороги, курс строго в +Z.
    CarState startingCar() const;

private:
    std::string theme_;
};

} // namespace rally
