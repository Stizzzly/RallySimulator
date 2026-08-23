#pragma once

#include <string>
#include <vector>

namespace rally {

struct ObjVector3 { float x, y, z; };
struct ObjVector2 { float u, v; };
struct ObjFace {
    int v1, v2, v3;
    int t1 = -1, t2 = -1, t3 = -1;
};

// Минимальный загрузчик Wavefront OBJ: поддерживает v, vt и треугольные f v/vt/vn.
class ObjModel {
public:
    std::vector<ObjVector3> vertices;
    std::vector<ObjVector2> texcoords;
    std::vector<ObjFace> faces;

    bool load(const std::string& path);
};

} // namespace rally
