#include "ObjModel.h"
#include <fstream>
#include <sstream>

namespace rally {

bool ObjModel::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "v") {
            ObjVector3 v; ss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        } else if (prefix == "vt") {
            ObjVector2 uv; ss >> uv.u >> uv.v;
            texcoords.push_back(uv);
        } else if (prefix == "f") {
            std::string s1, s2, s3;
            ss >> s1 >> s2 >> s3;
            auto parseIndex = [](const std::string& token, int& vertex, int& texcoord) {
                size_t slash = token.find('/');
                vertex = std::stoi(token.substr(0, slash)) - 1;
                texcoord = -1;
                if (slash == std::string::npos || slash + 1 >= token.size() || token[slash + 1] == '/') return;
                size_t nextSlash = token.find('/', slash + 1);
                texcoord = std::stoi(token.substr(slash + 1, nextSlash - slash - 1)) - 1;
            };
            ObjFace f;
            parseIndex(s1, f.v1, f.t1); parseIndex(s2, f.v2, f.t2); parseIndex(s3, f.v3, f.t3);
            faces.push_back(f);
        }
    }
    return true;
}

} // namespace rally
