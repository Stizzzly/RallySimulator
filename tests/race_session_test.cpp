#include "../src/domain/RaceSession.h"
#include <cassert>
int main() { rally::StageConfig stage; stage.length=100; stage.checkpointInterval=25; stage.goldTime=10; stage.silverTime=20; stage.bronzeTime=30; rally::RaceSession race; race.start(stage, {}); assert(race.checkpointCount()==4); assert(rally::RaceSession::medalFor(stage, 10)==rally::Medal::Gold); assert(rally::RaceSession::medalFor(stage, 25)==rally::Medal::Bronze); assert(race.updateProgress(25)); assert(race.checkpoint()==1); assert(race.updateProgress(100)); assert(race.isFinished()); return 0; }
