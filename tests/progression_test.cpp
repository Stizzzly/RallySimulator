#include "../src/application/Progression.h"
#include <cassert>

int main() {
    using namespace rally;

    // Новое лучшее время и медаль сохраняются; серебро золото не открывает.
    Profile p;
    RaceResult silver;
    silver.elapsed = 100.0f;
    silver.medal = Medal::Silver;
    silver.newBest = true;
    applyRaceResult(p, 0, silver);
    assert(p.bestTimes[0] == 100.0f);
    assert(p.medals[0] == Medal::Silver);
    assert(p.unlockedStages == 1);

    // Более медленный заезд не ухудшает лучшее время, но медаль может вырасти до золота.
    RaceResult gold;
    gold.elapsed = 110.0f;
    gold.medal = Medal::Gold;
    gold.newBest = false;
    applyRaceResult(p, 0, gold);
    assert(p.bestTimes[0] == 100.0f);
    assert(p.medals[0] == Medal::Gold);
    assert(p.unlockedStages == 2); // золото открывает следующий этап

    // Повторное золото не расширяет разблокировку дважды за один этап.
    applyRaceResult(p, 0, gold);
    assert(p.unlockedStages == 2);

    // Золото на последнем этапе не выходит за границы каталога.
    RaceResult last;
    last.elapsed = 5.0f;
    last.medal = Medal::Gold;
    last.newBest = true;
    applyRaceResult(p, 2, last);
    assert(p.unlockedStages == 2);

    // Некорректный индекс безопасен.
    applyRaceResult(p, -1, last);
    applyRaceResult(p, 7, last);
    assert(p.bestTimes[2] == 5.0f); // результат этапа 2 записан до этого
    assert(p.unlockedStages == 2);

    return 0;
}
