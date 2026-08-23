#include "Progression.h"

namespace rally {

void applyRaceResult(Profile& profile, int stageIndex, const RaceResult& result) {
    if (stageIndex < 0 || stageIndex >= static_cast<int>(profile.bestTimes.size())) return;

    if (result.newBest && result.elapsed > 0.0f)
        profile.bestTimes[static_cast<size_t>(stageIndex)] = result.elapsed;

    if (result.medal > profile.medals[static_cast<size_t>(stageIndex)])
        profile.medals[static_cast<size_t>(stageIndex)] = result.medal;

    const int stagesTotal = static_cast<int>(profile.bestTimes.size());
    if (result.medal == Medal::Gold && stageIndex + 1 < stagesTotal
        && profile.unlockedStages < stageIndex + 2)
        profile.unlockedStages = stageIndex + 2;
}

} // namespace rally
