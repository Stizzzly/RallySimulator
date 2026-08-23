#include "../src/domain/TrackModel.h"
#include <cassert>
#include <cmath>

int main() {
    using namespace rally;

    TrackModel mountain("mountain");
    TrackModel forest("forest");
    TrackModel coast("coast");

    // Ось дороги зависит от темы и детерминирована.
    assert(std::fabs(mountain.centerX(0.0f) - 10.0f) < 0.001f); // cos(0)*10
    assert(std::fabs(forest.centerX(100.0f) - coast.centerX(100.0f)) > 0.001f);

    // Дорога плоская: высота на оси = базовые холмы + подсыпка.
    const float z = 300.0f;
    const float expectedRoad = std::sin(z * 0.03f) * 3.0f + 0.2f;
    assert(std::fabs(mountain.heightAt(mountain.centerX(z), z) - expectedRoad) < 0.001f);

    // Покрытия: асфальт на дороге вне гравийных пятен, трава за обочиной,
    // гравий там, где шум превышает порог.
    assert(mountain.surfaceAt(mountain.centerX(300.0f), 300.0f) == SurfaceType::Asphalt);
    assert(mountain.surfaceAt(mountain.centerX(300.0f) + 20.0f, 300.0f) == SurfaceType::Grass);
    assert(std::sin(0.015f) + std::cos(0.045f) > 0.8f); // старт горы - гравийное пятно
    assert(mountain.surfaceAt(mountain.centerX(0.0f) + 5.0f, 0.0f) == SurfaceType::Gravel);

    // Стартовая позиция: центр дороги, курс строго в +Z.
    CarState start = mountain.startingCar();
    assert(std::fabs(start.x - mountain.centerX(0.0f)) < 0.001f);
    assert(std::fabs(start.y - mountain.heightAt(start.x, 0.0f)) < 0.001f);
    assert(std::fabs(start.heading - 180.0f) < 0.001f);

    return 0;
}
