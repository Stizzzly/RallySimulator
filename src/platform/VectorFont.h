#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace rally {

// Встроенный векторный (stroke) шрифт в духе аркад 70-х: глифы — ломаные линии
// на сетке шириной 4 и высотой 6 единиц. Никаких текстур и внешних ассетов.
class VectorFont {
public:
    using Point = std::pair<float, float>;
    using Polyline = std::vector<Point>;

    static const std::vector<Polyline>& glyph(char c);
    // Ширина ячейки символа в единицах сетки (глиф 4 + промежуток 2).
    static float advance() { return 6.0f; }

    // Ширина строки в единицах сетки.
    static float measure(const char* text);
};

} // namespace rally
