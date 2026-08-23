#pragma once

#include "../domain/RaceTypes.h"
#include "ProfileStore.h"

namespace rally {

// Правила прогресса профиля: лучшее время, лучшая медаль,
// разблокировка следующего этапа за золото (см. GDD).
void applyRaceResult(Profile& profile, int stageIndex, const RaceResult& result);

} // namespace rally
