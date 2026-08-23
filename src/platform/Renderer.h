#pragma once

#include "../domain/RaceTypes.h"
#include "../platform/ObjModel.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <vector>

namespace rally {

class Game;
class TrackModel;
struct CarState;

// Платформенный адаптер OpenGL 1.4 fixed pipeline.
// Вся презентация здесь: мир, частицы, камеры, HUD и меню.
class Renderer {
public:
    bool init(SDL_Window* window);
    void loadAssets();
    void release();

    // Полный кадр. Эффекты (камера, пыль, наклоны) обновляются только в гонке.
    void renderFrame(float deltaSeconds, const Game& game, float steerInput, float throttleInput);

private:
    struct Particle {
        float x, y, z;
        float vx, vy, vz;
        float life, maxLife;
        float size;
        float r, g, b, a;
        bool active = false;
    };

    // --- мир ---
    void rebuildTrack(const TrackModel& track, int lengthZ);
    void applyStageAtmosphere(const Game& game);
    void updateCamera(float dt, const CarState& car);
    void updateBodyPose(float dt, const CarState& car, const TrackModel& track,
                        const SurfaceType surface, float steerInput);
    void emitDust(float x, float y, float z, float r, float g, float b);
    void updateAndDrawParticles(float dt);

    // --- отрисовка ---
    void drawWorld(const Game& game);
    void drawCar(const Game& game);
    void drawGround(float carZ);
    void drawHud(const Game& game, float throttle, float dt);
    void drawStateUi(const Game& game);

    // --- приборная панель (в стиле аркадного референса) ---
    void drawTachometer(float cx, float cy, float r, float rpm01, char gearChar);
    void drawGearBox(float cx, float cy, float w, float h, char gearChar);
    void drawDigitalSpeed(float rightX, float baseline, float size, float kmh);
    void outlinedText(float x, float y, float size, const char* s, bool alignLeft = false);
    static float gaugeValueAngle(float value01); // угол стрелки на циферблате

    // --- интерфейс ---
    void text(float centerX, float y, float size, const char* s,
              float r, float g, float b, bool alignLeft = false);
    void textRight(float rightX, float y, float size, const char* s, float r, float g, float b);
    static std::string formatTime(float seconds);
    static const char* medalName(int medal);

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_ = nullptr;

    ObjModel model_;
    GLuint carTexture_ = 0;
    GLuint carList_ = 0;
    std::vector<float> trackData_; // xyz + rgb, stride 6
    Particle particles_[300];

    // Сглаженная камера и косметика кузова.
    float camX_ = 0, camY_ = 0, camZ_ = 0, camAngle_ = 0;
    float pitch_ = 0, roll_ = 0;
    float steerVisual_ = 0;
    float rpmVisual_ = 0, kmhVisual_ = 0; // стрелки приборов
    std::string builtTheme_;
};

} // namespace rally
