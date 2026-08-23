#include "StageFiles.h"
#include <fstream>
#include <sstream>

namespace rally {
namespace {
const char* kStagePaths[] = {
    "01_mountain.stage",
    "02_forest.stage",
    "03_coast.stage",
};
}

std::vector<StageConfig> loadStagesFromDirectory(const std::string& stagesDir) {
    std::vector<StageConfig> stages;
    for (const char* file : kStagePaths) {
        std::ifstream in(stagesDir + "/" + file);
        if (!in.is_open()) continue;
        std::ostringstream buffer;
        buffer << in.rdbuf();
        StageConfig stage;
        if (StageParser::parseText(buffer.str(), stage)) stages.push_back(stage);
    }
    return stages;
}

} // namespace rally
