#include "Renderer.h"
#include "../application/Game.h"
#include "../domain/RaceSession.h"
#include "../domain/TrackModel.h"
#include "VectorFont.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rally {
namespace {

constexpr float kDeg2Rad = 3.14159265f / 180.0f;
constexpr int kScreenWidth = 800;
constexpr int kScreenHeight = 600;

void setupPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    const GLdouble ymax = zNear * tan(fovy * 3.14159265 / 360.0);
    const GLdouble ymin = -ymax;
    const GLdouble xmin = ymin * aspect;
    const GLdouble xmax = ymax * aspect;
    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

// Магия Кармака: быстрый обратный корень для нормалей.
float qRsqrt(float number) {
    long i;
    float x2, y;
    const float threehalfs = 1.5F;
    x2 = number * 0.5F;
    y = number;
    i = *(long*)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float*)&i;
    y = y * (threehalfs - (x2 * y * y));
    return y;
}

GLuint loadBMPTexture(const char* path) {
    SDL_Surface* source = SDL_LoadBMP(path);
    if (!source) {
        SDL_Log("Texture not loaded (%s): %s", path, SDL_GetError());
        return 0;
    }
    // SDL хранит 24-битный BMP в BGR; конвертация исключает зависимость от
    // палитры и 32-битных форматов конкретного файла.
    SDL_Surface* pixels = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_BGR24, 0);
    SDL_FreeSurface(source);
    if (!pixels) return 0;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pixels->w, pixels->h, 0,
                 GL_BGR, GL_UNSIGNED_BYTE, pixels->pixels);
    SDL_Log("Loaded car texture: %s (%dx%d)", path, pixels->w, pixels->h);
    SDL_FreeSurface(pixels);
    return texture;
}

void drawWheel() {
    // 12 граней: достаточно кругло и всё ещё в духе ретро-рендера.
    const int segments = 12;
    const float radius = 0.38f;
    const float halfWidth = 0.18f;

    glColor3f(0.06f, 0.06f, 0.06f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float a = (float)i * 2.0f * 3.14159f / segments;
        float y = cosf(a) * radius;
        float z = sinf(a) * radius;
        glVertex3f(-halfWidth, y, z);
        glVertex3f(halfWidth, y, z);
    }
    glEnd();

    glColor3f(0.82f, 0.82f, 0.78f);
    for (float side : {-halfWidth - 0.005f, halfWidth + 0.005f}) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(side, 0.0f, 0.0f);
        for (int i = 0; i <= segments; ++i) {
            float a = (float)i * 2.0f * 3.14159f / segments;
            glVertex3f(side, cosf(a) * radius * 0.64f, sinf(a) * radius * 0.64f);
        }
        glEnd();
    }
}

} // namespace

bool Renderer::init(SDL_Window* window) {
    window_ = window;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    gl_ = SDL_GL_CreateContext(window);
    if (!gl_) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 40.0f);
    glFogf(GL_FOG_END, 110.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setupPerspective(68.0, (double)kScreenWidth / kScreenHeight, 0.1, 200.0);
    return true;
}

void Renderer::loadAssets() {
    model_.load("car.obj");
    carTexture_ = model_.vertices.empty() ? 0 : loadBMPTexture("lancia_delta_volterra.bmp");
    const bool texturedModel = carTexture_ != 0 && !model_.texcoords.empty();

    if (model_.faces.empty()) return; // фолбэк-машина рисуется процедурно

    carList_ = glGenLists(1);
    glNewList(carList_, GL_COMPILE);
    if (texturedModel) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, carTexture_);
    }
    glBegin(GL_TRIANGLES);
    for (const auto& f : model_.faces) {
        ObjVector3 v1 = model_.vertices[f.v1], v2 = model_.vertices[f.v2], v3 = model_.vertices[f.v3];

        // Нормаль треугольника
        float ux = v2.x - v1.x, uy = v2.y - v1.y, uz = v2.z - v1.z;
        float vx = v3.x - v1.x, vy = v3.y - v1.y, vz = v3.z - v1.z;
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        float invLen = qRsqrt(nx * nx + ny * ny + nz * nz);
        nx *= invLen; ny *= invLen; nz *= invLen;

        // Базовое освещение: свет сверху, бока темнее
        float brightness = 0.5f + (ny > 0 ? ny : 0) * 0.5f;
        if (nx > 0.5 || nx < -0.5) brightness *= 0.7f;

        // Раскраска по геометрии: ливрея читается даже без UV
        float r = 0.8f, g = 0.1f, b = 0.1f;
        float avgY = (v1.y + v2.y + v3.y) / 3.0f;
        float avgX = (v1.x + v2.x + v3.x) / 3.0f;
        float avgZ = (v1.z + v2.z + v3.z) / 3.0f;

        if (avgY > 0.85f) {
            if (ny < 0.9f) {
                r = 0.1f; g = 0.1f; b = 0.1f; // стекла
                brightness = 1.0f;
            } else {
                r = 0.9f; g = 0.9f; b = 0.9f; // крыша
            }
        }
        if (avgZ < -2.20f && nz < -0.7f) {
            if (avgY < 0.68f) {
                r = 0.88f; g = 0.88f; b = 0.84f; // белый нижний бампер
            } else if (avgY < 0.98f) {
                float ax = fabsf(avgX);
                if ((ax > 0.22f && ax < 0.56f) || ax > 0.67f) {
                    r = 0.85f; g = 0.82f; b = 0.65f; // четыре фары
                } else {
                    r = 0.04f; g = 0.04f; b = 0.04f; // разделенная решетка
                }
            }
        }
        if (avgZ > 2.08f && nz > 0.7f) {
            if (avgY < 0.68f) {
                r = 0.88f; g = 0.88f; b = 0.84f;
            } else if (avgY < 1.0f && fabsf(avgX) > 0.55f) {
                r = 0.85f; g = 0.08f; b = 0.03f; // квадратные задние фонари
            }
        }

        // При наличии UV/BMP текстура задает все детали ливреи.
        if (texturedModel) { r = 1.0f; g = 1.0f; b = 1.0f; }
        glColor3f(r * brightness, g * brightness, b * brightness);

        if (texturedModel && f.t1 >= 0 && f.t1 < (int)model_.texcoords.size()) glTexCoord2f(model_.texcoords[f.t1].u, 1.0f - model_.texcoords[f.t1].v);
        glVertex3f(v1.x, v1.y, v1.z);
        if (texturedModel && f.t2 >= 0 && f.t2 < (int)model_.texcoords.size()) glTexCoord2f(model_.texcoords[f.t2].u, 1.0f - model_.texcoords[f.t2].v);
        glVertex3f(v2.x, v2.y, v2.z);
        if (texturedModel && f.t3 >= 0 && f.t3 < (int)model_.texcoords.size()) glTexCoord2f(model_.texcoords[f.t3].u, 1.0f - model_.texcoords[f.t3].v);
        glVertex3f(v3.x, v3.y, v3.z);
    }
    glEnd();
    if (texturedModel) glDisable(GL_TEXTURE_2D);
    glEndList();
}

void Renderer::release() {
    if (carList_) glDeleteLists(carList_, 1);
    if (carTexture_) glDeleteTextures(1, &carTexture_);
    if (gl_) SDL_GL_DeleteContext(gl_);
    gl_ = nullptr;
}

void Renderer::applyStageAtmosphere(const Game& game) {
    const StageConfig& stage = game.activeStage();
    GLfloat fogColor[] = {stage.fogR, stage.fogG, stage.fogB, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glClearColor(stage.fogR, stage.fogG, stage.fogB, 1.0f);
}

void Renderer::rebuildTrack(const TrackModel& track, int lengthZ) {
    trackData_.clear();
    trackData_.reserve(static_cast<size_t>(((lengthZ + 20) / 4 + 1) * (80 / 4) * 6 * 6));

    for (int z = -20; z < lengthZ; z += 4) {
        const float centerX1 = track.centerX((float)z);
        const float centerX2 = track.centerX((float)(z + 4));

        for (int x = -40; x < 40; x += 4) {
            const float curX1[2] = {centerX1 + x, centerX1 + x + 4};
            const float curX2[2] = {centerX2 + x, centerX2 + x + 4};
            const float zs[2] = {(float)z, (float)(z + 4)};

            auto vertexColor = [&](float xp, float zp, float center, float out[3]) {
                const float dist = fabsf(xp - center);
                const SurfaceType surface = track.surfaceAt(xp, zp);
                if (surface == SurfaceType::Asphalt) {
                    const float grey = 0.25f + sinf(xp * 10.0f) * 0.02f;
                    if (dist < 0.4f && ((int(zp) / 4) % 2) == 0) { out[0] = 1; out[1] = 1; out[2] = 1; } // разметка
                    else { out[0] = grey; out[1] = grey; out[2] = grey + 0.05f; }
                } else if (surface == SurfaceType::Gravel) {
                    const float dirtNoise = (sinf(xp * 5.0f) * sinf(zp * 5.0f)) * 0.05f;
                    out[0] = 0.6f + dirtNoise;
                    out[1] = 0.5f + dirtNoise;
                    out[2] = 0.4f + dirtNoise;
                } else {
                    if (dist < TrackModel::kRoadWidth + TrackModel::kCurbWidth) { // поребрик
                        if (((int(zp) / 3) % 2) == 0) { out[0] = 0.9f; out[1] = 0.1f; out[2] = 0.1f; }
                        else { out[0] = 0.9f; out[1] = 0.9f; out[2] = 0.9f; }
                    } else {
                        if (((int(xp) / 4 + int(zp) / 4) % 2) == 0) { out[0] = 0.1f; out[1] = 0.4f; out[2] = 0.1f; }
                        else { out[0] = 0.2f; out[1] = 0.5f; out[2] = 0.2f; }
                    }
                }
            };

            float c1[3], c2[3], c3[3], c4[3];
            vertexColor(curX1[0], zs[0], centerX1, c1);
            vertexColor(curX1[1], zs[0], centerX1, c2);
            vertexColor(curX2[0], zs[1], centerX2, c3);
            vertexColor(curX2[1], zs[1], centerX2, c4);

            const float quad[4][6] = {
                {curX1[0], track.heightAt(curX1[0], zs[0]), zs[0], c1[0], c1[1], c1[2]},
                {curX1[1], track.heightAt(curX1[1], zs[0]), zs[0], c2[0], c2[1], c2[2]},
                {curX2[0], track.heightAt(curX2[0], zs[1]), zs[1], c3[0], c3[1], c3[2]},
                {curX2[1], track.heightAt(curX2[1], zs[1]), zs[1], c4[0], c4[1], c4[2]},
            };
            const int idx[6] = {0, 1, 2, 1, 3, 2};
            for (int i = 0; i < 6; ++i)
                for (int k = 0; k < 6; ++k)
                    trackData_.push_back(quad[idx[i]][k]);
        }
    }
    SDL_Log("Level RAM usage: %.2f MB", (float)(trackData_.size() * sizeof(float)) / 1048576.0f);
}

void Renderer::drawGround(float carZ) {
    if (trackData_.empty()) return;

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, 6 * sizeof(float), &trackData_[0]);
    glColorPointer(3, GL_FLOAT, 6 * sizeof(float), &trackData_[3]);

    // Отрисовка только видимого куска (culling).
    const int verticesPerRow = (80 / 4) * 6;
    const int currentRow = (int)(carZ + 20) / 4;
    int startVertex = (currentRow - 5) * verticesPerRow;
    if (startVertex < 0) startVertex = 0;

    int drawCount = 30 * verticesPerRow; // ~120 метров вперед
    if (startVertex + drawCount > (int)trackData_.size() / 6)
        drawCount = (int)trackData_.size() / 6 - startVertex;

    if (drawCount > 0) glDrawArrays(GL_TRIANGLES, startVertex, drawCount);

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void Renderer::updateCamera(float dt, const CarState& car) {
    camX_ += (car.x - camX_) * 10.0f * dt;
    camY_ += (car.y - camY_) * 10.0f * dt;
    camZ_ += (car.z - camZ_) * 10.0f * dt;
    float angleDiff = car.heading - camAngle_;
    while (angleDiff > 180.0f) angleDiff -= 360.0f;
    while (angleDiff < -180.0f) angleDiff += 360.0f;
    camAngle_ += angleDiff * 5.0f * dt;
}

void Renderer::updateBodyPose(float dt, const CarState& car, const TrackModel& track,
                              const SurfaceType surface, float steerInput) {
    const float rad = car.heading * kDeg2Rad;
    const float dirX = -sin(rad), dirZ = -cos(rad);
    const float noseY = track.heightAt(car.x + dirX * 2.0f, car.z + dirZ * 2.0f);
    const float sideY = track.heightAt(car.x - dirZ * 1.5f, car.z + dirX * 1.5f);
    const float targetPitch = (noseY - car.y) * 30.0f;
    float targetRoll = (sideY - car.y) * 30.0f;
    targetRoll += steerInput * fabsf(car.speed) * 0.3f;
    pitch_ += (targetPitch - pitch_) * 5.0f * dt;
    roll_ += (targetRoll - roll_) * 5.0f * dt;
    steerVisual_ += (steerInput - steerVisual_) * 8.0f * dt;
}

void Renderer::emitDust(float x, float y, float z, float r, float g, float b) {
    for (int i = 0; i < (int)(sizeof(particles_) / sizeof(particles_[0])); ++i) {
        Particle& p = particles_[i];
        if (p.active) continue;
        p.active = true;
        p.x = x + ((rand() % 100) / 100.0f - 0.5f);
        p.y = y;
        p.z = z + ((rand() % 100) / 100.0f - 0.5f);
        p.vx = ((rand() % 100) / 50.0f - 1.0f) * 3.0f;
        p.vy = ((rand() % 100) / 100.0f) * 4.0f + 1.0f;
        p.vz = ((rand() % 100) / 50.0f - 1.0f) * 3.0f;
        p.maxLife = 0.5f + ((rand() % 100) / 100.0f) * 0.5f;
        p.life = p.maxLife;
        p.size = 0.3f;
        p.r = r; p.g = g; p.b = b;
        break;
    }
}

void Renderer::updateAndDrawParticles(float dt) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // прозрачность без записи глубины

    for (int i = 0; i < (int)(sizeof(particles_) / sizeof(particles_[0])); ++i) {
        Particle& p = particles_[i];
        if (!p.active) continue;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        p.size += 3.0f * dt;
        p.life -= dt;
        p.a = (p.life / p.maxLife) * 0.6f;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }

        glPushMatrix();
        glTranslatef(p.x, p.y, p.z);
        glRotatef(-camAngle_, 0.0f, 1.0f, 0.0f); // билборд лицом к камере
        glColor4f(p.r, p.g, p.b, p.a);
        const float s = p.size;
        glBegin(GL_QUADS);
        glVertex3f(-s, -s, 0); glVertex3f(s, -s, 0);
        glVertex3f(s, s, 0); glVertex3f(-s, s, 0);
        glEnd();
        glPopMatrix();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::drawCar(const Game& game) {
    const CarState& car = game.session().car();

    glPushMatrix();
    // Импортированная Delta стоит колёсами на земле; процедурной нужен подъём.
    glTranslatef(car.x, car.y + (carList_ ? 0.0f : 0.4f), car.z);
    glRotatef(car.heading, 0.0f, 1.0f, 0.0f);
    glRotatef(pitch_, 1.0f, 0.0f, 0.0f);
    glRotatef(roll_, 0.0f, 0.0f, 1.0f);

    if (carList_) {
        // OBJ из Blender Z-up конвертируется в Y-up только при отрисовке.
        glPushMatrix();
        glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glCallList(carList_);
        glPopMatrix();
    } else {
        // Фолбэк: коробка + отдельные колёса с поворотом руля.
        glPushMatrix(); glScalef(0.9f, 0.6f, 2.0f);
        glBegin(GL_QUADS);
        glColor3f(0.8f, 0.0f, 0.0f); glVertex3f(-1,1,-1); glVertex3f(1,1,-1); glVertex3f(1,1,1); glVertex3f(-1,1,1);
        glColor3f(0.2f, 0.2f, 0.2f); glVertex3f(-1,0,-1); glVertex3f(1,0,-1); glVertex3f(1,0,1); glVertex3f(-1,0,1);
        glColor3f(0.6f, 0.0f, 0.0f); glVertex3f(-1,0,1); glVertex3f(1,0,1); glVertex3f(1,1,1); glVertex3f(-1,1,1);
        glVertex3f(-1,0,-1); glVertex3f(1,0,-1); glVertex3f(1,1,-1); glVertex3f(-1,1,-1);
        glVertex3f(-1,0,-1); glVertex3f(-1,0,1); glVertex3f(-1,1,1); glVertex3f(-1,1,-1);
        glVertex3f(1,0,-1); glVertex3f(1,0,1); glVertex3f(1,1,1); glVertex3f(1,1,-1);
        glEnd(); glPopMatrix();

        const float steerAngle = steerVisual_ * 30.0f;
        glPushMatrix(); glTranslatef(-1.0f, -0.1f, -1.35f);
        glRotatef(steerAngle, 0.0f, 1.0f, 0.0f); drawWheel(); glPopMatrix();
        glPushMatrix(); glTranslatef(1.0f, -0.1f, -1.35f);
        glRotatef(steerAngle, 0.0f, 1.0f, 0.0f); drawWheel(); glPopMatrix();
        glPushMatrix(); glTranslatef(-1.0f, -0.1f, 1.32f); drawWheel(); glPopMatrix();
        glPushMatrix(); glTranslatef(1.0f, -0.1f, 1.32f); drawWheel(); glPopMatrix();
    }
    glPopMatrix();
}

void Renderer::renderFrame(float deltaSeconds, const Game& game, float steerInput, float throttleInput) {
    const StageConfig& stage = game.activeStage();
    const CarState& car = game.session().car();
    const bool racing = game.state() == GameState::Race;

    if (builtTheme_ != stage.theme) {
        rebuildTrack(game.track(), stage.length);
        builtTheme_ = stage.theme;
        // Камера сразу за машиной, чтобы не догонять её на старте.
        camX_ = car.x; camY_ = car.y; camZ_ = car.z; camAngle_ = car.heading;
        pitch_ = roll_ = steerVisual_ = 0;
    }
    applyStageAtmosphere(game);

    if (racing) {
        updateCamera(deltaSeconds, car);
        const SurfaceType surface = game.track().surfaceAt(car.x, car.z);
        updateBodyPose(deltaSeconds, car, game.track(), surface, steerInput);

        // Пыль: на земле, быстро, дрифт на асфальте или любое покрытие кроме асфальта.
        const float speed = sqrtf(car.velocityX * car.velocityX + car.velocityZ * car.velocityZ);
        if (speed > 10.0f && car.y <= game.track().heightAt(car.x, car.z) + 0.1f) {
            const float rad = car.heading * kDeg2Rad;
            const float dirX = -sin(rad), dirZ = -cos(rad);
            const float dot = (car.velocityX / speed) * dirX + (car.velocityZ / speed) * dirZ;
            const bool asphaltDrift = surface == SurfaceType::Asphalt && dot < 0.95f;
            if (asphaltDrift || surface == SurfaceType::Gravel || surface == SurfaceType::Grass) {
                const float cr = surface == SurfaceType::Asphalt ? 0.7f : (surface == SurfaceType::Gravel ? 0.7f : 0.35f);
                const float cg = surface == SurfaceType::Asphalt ? 0.7f : (surface == SurfaceType::Gravel ? 0.6f : 0.25f);
                const float cb = surface == SurfaceType::Asphalt ? 0.7f : (surface == SurfaceType::Gravel ? 0.4f : 0.15f);
                const float rearX = car.x + dirX * 1.5f, rearZ = car.z + dirZ * 1.5f;
                const float rightX = cos(rad), rightZ = -sin(rad);
                const float halfWidth = 0.8f;
                const int perWheel = surface == SurfaceType::Gravel ? 4 : 2;
                for (int i = 0; i < perWheel; ++i) {
                    emitDust(rearX - rightX * halfWidth, car.y, rearZ - rightZ * halfWidth, cr, cg, cb);
                    emitDust(rearX + rightX * halfWidth, car.y, rearZ + rightZ * halfWidth, cr, cg, cb);
                }
            }
        }
    }

    // --- мир ---
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (game.hoodCamera()) {
        // Капотная камера: глаз над капотом, взгляд вперёд по сглаженному курсу.
        glTranslatef(0.0f, -1.15f, -0.25f);
        glRotatef(-camAngle_, 0.0f, 1.0f, 0.0f);
        glTranslatef(-car.x, -car.y, -car.z);
    } else {
        glTranslatef(0.0f, -2.2f, -5.0f);
        glRotatef(-camAngle_, 0.0f, 1.0f, 0.0f);
        glTranslatef(-camX_, -camY_, -camZ_);
    }

    if (game.state() != GameState::ProfileSelect && game.state() != GameState::StageSelect) {
        if (!game.hoodCamera()) drawCar(game);
        drawGround(car.z);
        updateAndDrawParticles(racing ? deltaSeconds : 0.0f);
    }

    // --- интерфейс ---
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, kScreenWidth, kScreenHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);

    if (game.state() == GameState::Race || game.state() == GameState::Pause
        || game.state() == GameState::Results)
        drawHud(game, throttleInput, deltaSeconds);
    drawStateUi(game);

    glEnable(GL_FOG);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// --- интерфейс ---

void Renderer::text(float centerX, float y, float size, const char* s,
                    float r, float g, float b, bool alignLeft) {
    glColor3f(r, g, b);
    glLineWidth(2.0f);
    const float unit = size / 6.0f;
    float x = alignLeft ? centerX : centerX - VectorFont::measure(s) * unit * 0.5f;
    for (const char* p = s; *p; ++p) {
        for (const auto& poly : VectorFont::glyph(*p)) {
            glBegin(GL_LINE_STRIP);
            for (const auto& pt : poly)
                glVertex2f(x + pt.first * unit, y - pt.second * unit); // орто с y-вниз
            glEnd();
        }
        x += VectorFont::advance() * unit;
    }
}

void Renderer::textRight(float rightX, float y, float size, const char* s, float r, float g, float b) {
    const float width = VectorFont::measure(s) * (size / 6.0f);
    text(rightX - width, y, size, s, r, g, b, true);
}

std::string Renderer::formatTime(float seconds) {
    if (seconds <= 0.0f) return "--:--.-";
    const int totalTenths = (int)(seconds * 10.0f);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d.%d",
                  totalTenths / 600, (totalTenths / 10) % 60, totalTenths % 10);
    return buf;
}

const char* Renderer::medalName(int medal) {
    switch (medal) {
    case 3: return "GOLD";
    case 2: return "SILVER";
    case 1: return "BRONZE";
    default: return "NONE";
    }
}

void medalColor(int medal, float& r, float& g, float& b) {
    switch (medal) {
    case 3: r = 1.00f; g = 0.85f; b = 0.20f; break;
    case 2: r = 0.80f; g = 0.82f; b = 0.88f; break;
    case 1: r = 0.85f; g = 0.55f; b = 0.35f; break;
    default: r = 0.55f; g = 0.58f; b = 0.62f; break;
    }
}

// --- приборная панель ---

// Дуга циферблата: 0 слева-снизу (210°), максимум справа-снизу (-30°), 240° хода.
float Renderer::gaugeValueAngle(float value01) {
    value01 = std::max(0.0f, std::min(1.0f, value01));
    return (210.0f - 240.0f * value01) * kDeg2Rad;
}

void Renderer::outlinedText(float x, float y, float size, const char* s, bool alignLeft) {
    static const float kOffsets[4][2] = {{1.5f, 1.5f}, {-1.5f, 1.5f}, {1.5f, -1.5f}, {-1.5f, -1.5f}};
    for (int i = 0; i < 4; ++i)
        text(x + kOffsets[i][0], y + kOffsets[i][1], size, s, 0.0f, 0.0f, 0.0f, alignLeft);
    text(x, y, size, s, 1.0f, 1.0f, 1.0f, alignLeft);
}

void Renderer::drawTachometer(float cx, float cy, float r, float rpm01, char gearChar) {
    (void)gearChar;
    const float maxRpm = 9000.0f;

    // Светлый непрозрачный циферблат, как на аркадном референсе.
    glColor3f(0.87f, 0.88f, 0.91f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 48; ++i) {
        const float a = (float)i / 48.0f * 2.0f * 3.14159f;
        glVertex2f(cx + cosf(a) * r, cy - sinf(a) * r);
    }
    glEnd();

    // Красная зона на ободе: оранжевая, переходящая в красную (6.3 -> 9).
    glLineWidth(6.0f);
    glBegin(GL_LINE_STRIP);
    glColor3f(1.0f, 0.55f, 0.10f);
    for (int i = 0; i <= 8; ++i) {
        const float a = gaugeValueAngle((6.3f + (7.6f - 6.3f) * i / 8.0f) / maxRpm);
        glVertex2f(cx + cosf(a) * (r - 2.0f), cy - sinf(a) * (r - 2.0f));
    }
    glEnd();
    glBegin(GL_LINE_STRIP);
    glColor3f(0.90f, 0.12f, 0.05f);
    for (int i = 0; i <= 8; ++i) {
        const float a = gaugeValueAngle((7.6f + (9.0f - 7.6f) * i / 8.0f) / maxRpm);
        glVertex2f(cx + cosf(a) * (r - 2.0f), cy - sinf(a) * (r - 2.0f));
    }
    glEnd();

    // Белый обод поверх дуги.
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 48; ++i) {
        const float a = (float)i / 48.0f * 2.0f * 3.14159f;
        glVertex2f(cx + cosf(a) * r, cy - sinf(a) * r);
    }
    glEnd();

    // Шкала: чёрные деления каждую 1000 с тёмной цифрой, мелкие каждые 500.
    for (int k = 0; k <= 9; ++k) {
        const float a = gaugeValueAngle((float)k / 9.0f);
        const float ca = cosf(a), sa = sinf(a);
        glColor3f(0.05f, 0.05f, 0.08f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(cx + ca * r * 0.84f, cy - sa * r * 0.84f);
        glVertex2f(cx + ca * r * 0.97f, cy - sa * r * 0.97f);
        glEnd();

        char label[2] = {(char)('0' + k), 0};
        const float size = 13.0f;
        text(cx + ca * r * 0.64f, cy - sa * r * 0.64f + 0.5f * size, size, label,
             0.05f, 0.05f, 0.08f);

        if (k < 9) {
            const float am = gaugeValueAngle((float)k / 9.0f + 0.5f / 9.0f);
            const float cm = cosf(am), sm = sinf(am);
            glLineWidth(1.0f);
            glBegin(GL_LINES);
            glVertex2f(cx + cm * r * 0.90f, cy - sm * r * 0.90f);
            glVertex2f(cx + cm * r * 0.97f, cy - sm * r * 0.97f);
            glEnd();
        }
    }

    // Толстая красно-оранжевая стрелка с небольшим противоходом.
    const float a = gaugeValueAngle(rpm01);
    glColor3f(1.0f, 0.22f, 0.05f);
    glLineWidth(5.0f);
    glBegin(GL_LINES);
    glVertex2f(cx - cosf(a) * r * 0.10f, cy + sinf(a) * r * 0.10f);
    glVertex2f(cx + cosf(a) * r * 0.82f, cy - sinf(a) * r * 0.82f);
    glEnd();

    // Тёмная ступица.
    glColor3f(0.08f, 0.08f, 0.10f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 16; ++i) {
        const float t = (float)i / 16.0f * 2.0f * 3.14159f;
        glVertex2f(cx + cosf(t) * 5.0f, cy - sinf(t) * 5.0f);
    }
    glEnd();
}

void Renderer::drawGearBox(float cx, float cy, float w, float h, char gearChar) {
    const float x0 = cx - w * 0.5f, y0 = cy - h * 0.5f;

    glColor3f(0.04f, 0.04f, 0.06f);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0); glVertex2f(x0 + w, y0);
    glVertex2f(x0 + w, y0 + h); glVertex2f(x0, y0 + h);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x0, y0); glVertex2f(x0 + w, y0);
    glVertex2f(x0 + w, y0 + h); glVertex2f(x0, y0 + h);
    glEnd();

    // Передача крупно вверху, SHIFT мелко внизу — как на референсе.
    char gear[2] = {gearChar, 0};
    text(cx, cy - 1.0f, 18, gear, 1, 1, 1);
    text(cx, cy + h * 0.5f - 7.0f, 8, "SHIFT", 0.9f, 0.9f, 0.95f);
}

void Renderer::drawDigitalSpeed(float rightX, float baseline, float size, float kmh) {
    // Три цифры с ведущими нулями: 007, 098, 198.
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%03d", (int)kmh);
    const float digitsWidth = VectorFont::measure(buf) * (size / 6.0f);
    const float unitSize = size * 0.4f;
    const float unitWidth = VectorFont::measure("KM/H") * (unitSize / 6.0f);

    // Блок выравнивается правым краем: цифры, за ними KM/H.
    const float startX = rightX - unitWidth - 8.0f - digitsWidth;
    outlinedText(startX, baseline, size, buf);
    outlinedText(startX + digitsWidth + 8.0f, baseline, unitSize, "KM/H");
}

void Renderer::drawHud(const Game& game, float throttle, float dt) {
    const RaceSession& s = game.session();

    // Таймер
    text(20, 14, 14, "TIME", 0.8f, 0.9f, 1.0f, true);
    text(20, 52, 34, formatTime(s.elapsed()).c_str(), 1, 1, 1, true);

    // Лучшее время
    text(780, 14, 14, "BEST", 0.8f, 0.9f, 1.0f, false);
    textRight(780, 52, 34, formatTime(game.bestTimeOfSelectedStage()).c_str(), 1, 1, 1);

    // Чекпоинты
    char cp[32];
    std::snprintf(cp, sizeof(cp), "CP %d/%d", s.checkpoint(), s.checkpointCount());
    text(kScreenWidth / 2, 26, 22, cp, 0.95f, 0.95f, 0.7f);

    // Приборный кластер: скорость цифрами слева, тахометр правее, под ним бокс передачи.
    const CarState& car = s.car();
    const float kmh = sqrtf(car.velocityX * car.velocityX + car.velocityZ * car.velocityZ) * 3.6f;

    // Аркадный RPM: 4 передачи с диапазонами по скорости.
    static const float kGearBand[5] = {0.0f, 45.0f, 90.0f, 140.0f, 241.0f};
    int gear = 1;
    while (gear < 4 && kmh >= kGearBand[gear]) ++gear;
    float rpm = 1000.0f + (kmh - kGearBand[gear - 1]) / (kGearBand[gear] - kGearBand[gear - 1]) * 7000.0f;
    if (kmh < 0.5f && throttle <= 0.1f) rpm = 900.0f;               // холостой
    if (kmh < 2.0f && throttle > 0.1f) rpm = 2500.0f + throttle * 2500.0f; // старт с места

    // Стрелка и цифры не дёргаются: сглаживание.
    const float k = std::min(1.0f, dt * 10.0f);
    rpmVisual_ += (rpm - rpmVisual_) * k;
    kmhVisual_ += (kmh - kmhVisual_) * k;

    char gearChar = (char)('0' + gear);
    if (car.speed < -0.5f) gearChar = 'R';
    drawTachometer(690, 420, 76, rpmVisual_ / 9000.0f, gearChar);
    drawGearBox(690, 522, 46, 42, gearChar);
    drawDigitalSpeed(606, 543, 34, kmhVisual_);

    // Всплеск чекпоинта/финиша
    if (game.checkpointFlash() > 0.0f && !game.finishedThisRun()) {
        char msg[32];
        std::snprintf(msg, sizeof(msg), "CHECKPOINT %d/%d", s.checkpoint(), s.checkpointCount());
        text(kScreenWidth / 2, 170, 30, msg, 0.4f, 1.0f, 0.5f);
    } else if (game.finishedThisRun() || s.isFinished()) {
        text(kScreenWidth / 2, 170, 42, "FINISH!", 0.3f, 1.0f, 0.45f);
    }
}

void Renderer::drawStateUi(const Game& game) {
    switch (game.state()) {
    case GameState::ProfileSelect:
        text(kScreenWidth / 2, 190, 64, "VOLTERRA RACING", 1.0f, 0.25f, 0.15f);
        text(kScreenWidth / 2, 250, 18, "ARCADE RALLY TIME ATTACK", 0.9f, 0.95f, 1.0f);
        if (SDL_GetTicks() % 900 < 550)
            text(kScreenWidth / 2, 380, 24, "PRESS ENTER", 1, 1, 1);
        text(kScreenWidth / 2, 570, 14,
             "ARROWS/WASD DRIVE   C CAMERA   ESC BACK", 0.75f, 0.8f, 0.85f);
        break;

    case GameState::StageSelect: {
        text(kScreenWidth / 2, 70, 36, "SELECT STAGE", 1, 1, 1);

        const std::vector<StageConfig>& stages = game.stages();
        const Profile& profile = game.profile();
        const size_t rows = std::min(stages.size(), profile.medals.size());
        float y = 200;
        for (size_t i = 0; i < rows; ++i) {
            const bool selected = (int)i == game.selectedStage();
            const bool unlocked = (int)i < profile.unlockedStages;
            float mr, mg, mb;
            medalColor(static_cast<int>(profile.medals[i]), mr, mg, mb);

            char best[32];
            std::snprintf(best, sizeof(best), "%s",
                          formatTime(profile.bestTimes[i]).c_str());

            char row[64];
            std::snprintf(row, sizeof(row), "%s%s",
                          selected ? "> " : "  ", stages[i].name.c_str());

            if (!unlocked)
                text(180, y, 28, row, 0.45f, 0.48f, 0.52f, true);
            else if (selected)
                text(180, y, 28, row, 1, 1, 1, true);
            else
                text(180, y, 28, row, 0.75f, 0.78f, 0.82f, true);

            text(500, y, 20, unlocked ? medalName(static_cast<int>(profile.medals[i])) : "LOCKED",
                 mr, mg, mb, true);
            if (unlocked) textRight(760, y + 2, 18, best, 0.8f, 0.85f, 0.9f);
            y += 90;
        }

        text(kScreenWidth / 2, 570, 14,
             "UP/DOWN SELECT   ENTER START   ESC BACK", 0.75f, 0.8f, 0.85f);
        break;
    }

    case GameState::Pause: {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 0.05f, 0.12f, 0.55f);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(kScreenWidth, 0);
        glVertex2f(kScreenWidth, kScreenHeight); glVertex2f(0, kScreenHeight);
        glEnd();
        glDisable(GL_BLEND);

        text(kScreenWidth / 2, 260, 48, "PAUSED", 1, 1, 1);
        text(kScreenWidth / 2, 330, 16, "ENTER RESUME      R RESTART", 0.85f, 0.9f, 0.95f);
        break;
    }

    case GameState::Results: {
        // Панель результатов поверх застывшего мира.
        const float bx = 140, by = 140, bw = kScreenWidth - 280, bh = 300;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.02f, 0.06f, 0.12f, 0.72f);
        glBegin(GL_QUADS);
        glVertex2f(bx, by); glVertex2f(bx + bw, by);
        glVertex2f(bx + bw, by + bh); glVertex2f(bx, by + bh);
        glEnd();
        glDisable(GL_BLEND);

        const StageConfig& stage = game.activeStage();
        const RaceResult& res = game.result();

        text(kScreenWidth / 2, by + 46, 30, stage.name.c_str(), 1.0f, 0.85f, 0.3f);

        char line[64];
        std::snprintf(line, sizeof(line), "TIME %s", formatTime(res.elapsed).c_str());
        text(kScreenWidth / 2, by + 110, 26, line, 1, 1, 1);
        std::snprintf(line, sizeof(line), "BEST %s", formatTime(game.bestTimeOfSelectedStage()).c_str());
        text(kScreenWidth / 2, by + 152, 20, line, 0.85f, 0.9f, 0.95f);

        float mr, mg, mb;
        medalColor(static_cast<int>(res.medal), mr, mg, mb);
        std::snprintf(line, sizeof(line), "MEDAL %s", medalName(static_cast<int>(res.medal)));
        text(kScreenWidth / 2, by + 204, 24, line, mr, mg, mb);

        if (res.newBest && SDL_GetTicks() % 700 < 450)
            text(kScreenWidth / 2, by + 256, 24, "NEW RECORD!", 0.35f, 1.0f, 0.45f);

        text(kScreenWidth / 2, by + bh - 14, 14, "PRESS ENTER", 0.75f, 0.8f, 0.85f);
        break;
    }

    case GameState::Race:
    default:
        break;
    }
}

} // namespace rally
