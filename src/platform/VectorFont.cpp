#include "VectorFont.h"
#include <cstring>
#include <map>

namespace rally {

namespace {

using P = VectorFont::Point;
using Poly = VectorFont::Polyline;

std::vector<Poly> lines(std::initializer_list<Poly> ls) { return std::vector<Poly>(ls); }

// Таблица глифов: сетка 4x6, начало координат — левый нижний угол.
std::map<char, std::vector<Poly>> buildTable() {
    std::map<char, std::vector<Poly>> t;
    t[' '] = {};
    t['A'] = lines({ Poly{P{0,0},P{0,4},P{2,6},P{4,4},P{4,0}}, Poly{P{0,2},P{4,2}} });
    t['B'] = lines({ Poly{P{0,0},P{0,6}}, Poly{P{0,6},P{3,6},P{4,4.5f},P{3,3},P{0,3}}, Poly{P{0,0},P{3,0},P{4,1.5f},P{3,3}} });
    t['C'] = lines({ Poly{P{4,4.5f},P{3,6},P{1,6},P{0,5},P{0,1},P{1,0},P{3,0},P{4,1.5f}} });
    t['D'] = lines({ Poly{P{0,0},P{0,6},P{2,6},P{4,4},P{4,2},P{2,0},P{0,0}} });
    t['E'] = lines({ Poly{P{4,6},P{0,6},P{0,0},P{4,0}}, Poly{P{0,3},P{3,3}} });
    t['F'] = lines({ Poly{P{4,6},P{0,6},P{0,0}}, Poly{P{0,3},P{3,3}} });
    t['G'] = lines({ Poly{P{4,4.5f},P{3,6},P{1,6},P{0,5},P{0,1},P{1,0},P{3,0},P{4,1.5f},P{4,3},P{2,3}} });
    t['H'] = lines({ Poly{P{0,0},P{0,6}}, Poly{P{4,0},P{4,6}}, Poly{P{0,3},P{4,3}} });
    t['I'] = lines({ Poly{P{1,6},P{3,6}}, Poly{P{2,6},P{2,0}}, Poly{P{1,0},P{3,0}} });
    t['J'] = lines({ Poly{P{4,6},P{4,1},P{3,0},P{1,0},P{0,1}} });
    t['K'] = lines({ Poly{P{0,0},P{0,6}}, Poly{P{4,6},P{0,3},P{4,0}} });
    t['L'] = lines({ Poly{P{0,6},P{0,0},P{4,0}} });
    t['M'] = lines({ Poly{P{0,0},P{0,6},P{2,3.5f},P{4,6},P{4,0}} });
    t['N'] = lines({ Poly{P{0,0},P{0,6},P{4,0},P{4,6}} });
    t['O'] = lines({ Poly{P{1,0},P{3,0},P{4,1},P{4,5},P{3,6},P{1,6},P{0,5},P{0,1},P{1,0}} });
    t['P'] = lines({ Poly{P{0,0},P{0,6},P{3,6},P{4,5},P{4,4.5f},P{3,3},P{0,3}} });
    t['Q'] = lines({ Poly{P{1,0},P{3,0},P{4,1},P{4,5},P{3,6},P{1,6},P{0,5},P{0,1},P{1,0}}, Poly{P{2.5f,2},P{4,0}} });
    t['R'] = lines({ Poly{P{0,0},P{0,6},P{3,6},P{4,5},P{4,4.5f},P{3,3},P{0,3}}, Poly{P{1.5f,3},P{4,0}} });
    t['S'] = lines({ Poly{P{4,5},P{3,6},P{1,6},P{0,5},P{0,4},P{1,3},P{3,3},P{4,2},P{4,1},P{3,0},P{1,0},P{0,1}} });
    t['T'] = lines({ Poly{P{0,6},P{4,6}}, Poly{P{2,6},P{2,0}} });
    t['U'] = lines({ Poly{P{0,6},P{0,1},P{1,0},P{3,0},P{4,1},P{4,6}} });
    t['V'] = lines({ Poly{P{0,6},P{2,0},P{4,6}} });
    t['W'] = lines({ Poly{P{0,6},P{1,0},P{2,2.5f},P{3,0},P{4,6}} });
    t['X'] = lines({ Poly{P{0,0},P{4,6}}, Poly{P{0,6},P{4,0}} });
    t['Y'] = lines({ Poly{P{0,6},P{2,3},P{4,6}}, Poly{P{2,3},P{2,0}} });
    t['Z'] = lines({ Poly{P{0,6},P{4,6},P{0,0},P{4,0}} });
    t['0'] = lines({ Poly{P{0,0},P{4,0},P{4,6},P{0,6},P{0,0}}, Poly{P{0,0},P{4,6}} });
    t['1'] = lines({ Poly{P{2,0},P{2,6}}, Poly{P{0.5f,5},P{2,6}} });
    t['2'] = lines({ Poly{P{0,5},P{1,6},P{3,6},P{4,5},P{4,4},P{0,0},P{4,0}} });
    t['3'] = lines({ Poly{P{0,6},P{3,6},P{4,5},P{3,4},P{1,3},P{3,2},P{4,1},P{3,0},P{0,0}} });
    t['4'] = lines({ Poly{P{0,6},P{0,2},P{4,2}}, Poly{P{4,6},P{4,0}} });
    t['5'] = lines({ Poly{P{4,6},P{0,6},P{0,3},P{4,3},P{4,0},P{0,0}} });
    t['6'] = lines({ Poly{P{4,6},P{0,6},P{0,0},P{4,0},P{4,3},P{0,3}} });
    t['7'] = lines({ Poly{P{0,6},P{4,6},P{1,0}} });
    t['8'] = lines({ Poly{P{0,0},P{0,6},P{4,6},P{4,0},P{0,0}}, Poly{P{0,3},P{4,3}} });
    t['9'] = lines({ Poly{P{4,0},P{4,6},P{0,6},P{0,3},P{4,3}} });
    t['-'] = lines({ Poly{P{0.5f,3},P{3.5f,3}} });
    t['.'] = lines({ Poly{P{2,0.2f},P{2,0}} });
    t[':'] = lines({ Poly{P{2,1},P{2,1.3f}}, Poly{P{2,4},P{2,4.3f}} });
    t['/'] = lines({ Poly{P{0,0},P{4,6}} });
    t['!'] = lines({ Poly{P{2,6},P{2,1.8f}}, Poly{P{2,1},P{2,0.7f}} });
    t['>'] = lines({ Poly{P{0,6},P{3,3},P{0,0}} });
    t['%'] = lines({
        Poly{P{0,0},P{4,6}},
        Poly{P{0,4.5f},P{0,6},P{1.5f,6},P{1.5f,4.5f},P{0,4.5f}},
        Poly{P{2.5f,0},P{2.5f,1.5f},P{4,1.5f},P{4,0},P{2.5f,0}} });
    return t;
}

} // namespace

const std::vector<VectorFont::Polyline>& VectorFont::glyph(char c) {
    static const std::map<char, std::vector<Polyline>> table = buildTable();
    static const std::vector<Polyline> empty;
    auto it = table.find(c);
    return it != table.end() ? it->second : empty;
}

float VectorFont::measure(const char* text) {
    if (!text) return 0.0f;
    return static_cast<float>(std::strlen(text)) * advance();
}

} // namespace rally
