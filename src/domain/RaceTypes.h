#pragma once

#include <string>

namespace rally {

enum class SurfaceType { Asphalt, Gravel, Grass };
enum class Medal { None = 0, Bronze = 1, Silver = 2, Gold = 3 };

struct StageConfig {
    std::string id;
    std::string name;
    std::string theme;
    int length = 7200;
    int checkpointInterval = 1200;
    float goldTime = 132.0f;
    float silverTime = 156.0f;
    float bronzeTime = 186.0f;
    // Атмосфера этапа (линейный туман рендера).
    float fogR = 0.42f;
    float fogG = 0.60f;
    float fogB = 0.86f;
};

struct CarState {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    float heading = 180.0f;
    float speed = 0.0f;
};

struct DriveInput { float steering = 0.0f; float throttle = 0.0f; float brake = 0.0f; };
struct RaceResult { float elapsed = 0.0f; Medal medal = Medal::None; bool newBest = false; };

} // namespace rally
