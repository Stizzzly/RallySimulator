#pragma once

#include "../application/StageParser.h"
#include "../domain/RaceTypes.h"
#include <string>
#include <vector>

namespace rally {

// Файловый адаптер каталога этапов: читает stages/*.stage и прогоняет через StageParser.
std::vector<StageConfig> loadStagesFromDirectory(const std::string& stagesDir);

} // namespace rally
