#include "../src/application/StageParser.h"
#include <cassert>

int main() {
    using namespace rally;

    const std::string text =
        "name=Test Stage\n"
        "theme=forest\n"
        "length=1000\n"
        "checkpoint_interval=250\n"
        "gold=60.5\n"
        "silver=75.25\n"
        "bronze=90\n"
        "fog_r=0.1\nfog_g=0.2\nfog_b=0.3\n"
        "unknown_key=42\n";

    StageConfig stage;
    assert(StageParser::parseText(text, stage));
    assert(stage.name == "Test Stage");
    assert(stage.theme == "forest");
    assert(stage.length == 1000);
    assert(stage.checkpointInterval == 250);
    assert(stage.goldTime > 60.4f && stage.goldTime < 60.6f);
    assert(stage.silverTime > 75.2f && stage.silverTime < 75.3f);
    assert(stage.bronzeTime == 90.0f);
    assert(stage.fogR == 0.1f && stage.fogG == 0.2f && stage.fogB == 0.3f);

    // Пустой/некорректный текст не даёт валидный этап.
    StageConfig broken;
    assert(!StageParser::parseText("unknown=1\n", broken));

    return 0;
}
