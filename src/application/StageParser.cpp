#include "StageParser.h"
#include <cstdlib>
#include <sstream>

namespace rally {

void StageParser::parseLine(const std::string& key, const std::string& value, StageConfig& stage) {
    if (key == "name") stage.name = value;
    else if (key == "theme") stage.theme = value;
    else if (key == "id") stage.id = value;
    else if (key == "length") stage.length = std::atoi(value.c_str());
    else if (key == "gold") stage.goldTime = static_cast<float>(std::atof(value.c_str()));
    else if (key == "silver") stage.silverTime = static_cast<float>(std::atof(value.c_str()));
    else if (key == "bronze") stage.bronzeTime = static_cast<float>(std::atof(value.c_str()));
    else if (key == "checkpoint_interval") stage.checkpointInterval = std::atoi(value.c_str());
    else if (key == "fog_r") stage.fogR = static_cast<float>(std::atof(value.c_str()));
    else if (key == "fog_g") stage.fogG = static_cast<float>(std::atof(value.c_str()));
    else if (key == "fog_b") stage.fogB = static_cast<float>(std::atof(value.c_str()));
}

bool StageParser::parseText(const std::string& text, StageConfig& stage) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        const size_t equal = line.find('=');
        if (equal == std::string::npos) continue;
        parseLine(line.substr(0, equal), line.substr(equal + 1), stage);
    }
    return !stage.name.empty();
}

} // namespace rally
