#include <tiny3d.h>
#include <matrix.h>

#include "obj_loader.h"

#include <SDL2/SDL.h>
#include <io/pad.h>
#include <sys/process.h>
#include <sys/systime.h>
#include <sysutil/sysutil.h>
#include <ppu-types.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

SYS_PROCESS_PARAM(1001, 0x100000);

static const float PI = 3.14159265358979323846f;
static const float DEG2RAD = PI / 180.0f;
static const float ROAD_WIDTH = 10.0f;
static const float CURB_WIDTH = 1.5f;
static const int LEVEL_LENGTH = 4000;
static const float CAR_RIDE_HEIGHT = 0.0f;
static const float CAR_MESH_SCALE = 1.0f;

// tiny3d's public colour constants are packed numerically as RGBA:
// 0xRRGGBBAA. The RSX byte stream looks like ABGR on little-endian PPU,
// but callers must use tiny3d's RGBA packing here.
static inline u32 rgba(u8 r, u8 g, u8 b, u8 a = 255)
{
    return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a;
}

struct TerrainVertex
{
    float x, y, z;
    u32 color;
};

struct Particle
{
    float x, y, z;
    float vx, vy, vz;
    float life, maxLife, size;
    u8 r, g, b;
    bool active;
};

struct Car
{
    float x = 0.0f, y = 1.0f, z = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    float angle = 0.0f, pitch = 0.0f, roll = 0.0f;
    float speed = 0.0f;
    float acceleration = 35.0f;
    float maxSpeed = 55.0f;
    float turnSpeed = 100.0f;
    float frictionAsphalt = 1.5f;
    float frictionGrass = 5.0f;
};

static std::vector<TerrainVertex> trackData;
static Particle particles[300];
static ObjMesh carMesh = {0};
static bool carMeshLoaded = false;
static u64 systemTimeMicros()
{
    u64 seconds = 0;
    u64 nanoseconds = 0;
    sysGetCurrentTime(&seconds, &nanoseconds);
    return seconds * 1000000ULL + nanoseconds / 1000ULL;
}

static float trackCenterX(float z)
{
    return sinf(z * 0.015f) * 40.0f
         + sinf(z * 0.04f) * 20.0f
         + cosf(z * 0.08f) * 10.0f;
}

static float terrainHeight(float x, float z)
{
    const float center = trackCenterX(z);
    const float dist = fabsf(x - center);
    const float base = sinf(z * 0.03f) * 3.0f;

    if (dist < ROAD_WIDTH + CURB_WIDTH) return base + 0.2f;
    if (dist < ROAD_WIDTH + CURB_WIDTH + 25.0f)
        return base + 0.2f + sinf(x * 0.4f) * 0.3f;

    const float wallDist = dist - (ROAD_WIDTH + CURB_WIDTH + 25.0f);
    return base + 0.2f + wallDist * wallDist * 0.05f + sinf(x) * 0.5f;
}

static int surfaceType(float x, float z)
{
    const float dist = fabsf(x - trackCenterX(z));
    if (dist > ROAD_WIDTH + 0.5f) return 2; // grass
    if (sinf(z * 0.015f) + cosf(z * 0.045f) > 0.8f) return 1; // gravel
    return 0; // asphalt
}

static u32 terrainColor(float x, float z, float center)
{
    const float dist = fabsf(x - center);
    const int surface = surfaceType(x, z);

    if (surface == 0 || surface == 1) {
        const int tileX = (int)floorf(x / 4.0f);
        const int tileZ = (int)floorf(z / 4.0f);
        const bool lightTile = ((tileX + tileZ) & 1) == 0;
        return lightTile ? rgba(205, 205, 205) : rgba(68, 70, 74);
    }

    if (dist < ROAD_WIDTH + CURB_WIDTH) {
        return (((int)z / 3) % 2 == 0) ? rgba(220, 35, 35) : rgba(235, 235, 235);
    }
    return (((int)x / 4 + (int)z / 4) % 2 == 0)
        ? rgba(28, 105, 35) : rgba(45, 135, 52);
}

static void addTriangle(float x1, float z1, float x2, float z2,
                        float x3, float z3, u32 color)
{
    TerrainVertex a = {x1, terrainHeight(x1, z1), z1, color};
    TerrainVertex b = {x2, terrainHeight(x2, z2), z2, color};
    TerrainVertex c = {x3, terrainHeight(x3, z3), z3, color};
    trackData.push_back(a);
    trackData.push_back(b);
    trackData.push_back(c);
}

static void generateLevel()
{
    const int stepX = 4;
    const int stepZ = 4;
    trackData.reserve((LEVEL_LENGTH / stepZ + 5) * 20 * 6);

    for (int z = -20; z < LEVEL_LENGTH; z += stepZ) {
        const float center1 = trackCenterX((float)z);
        const float center2 = trackCenterX((float)(z + stepZ));
        for (int x = -40; x < 40; x += stepX) {
            const float x1 = center1 + x;
            const float x2 = center2 + x;
            const float sampleZ = (float)z + stepZ * 0.5f;
            const float sampleX = (x1 + (x1 + stepX) + x2 + (x2 + stepX)) * 0.25f;
            const u32 cellColor = terrainColor(sampleX, sampleZ, trackCenterX(sampleZ));
            addTriangle(x1, (float)z, x1 + stepX, (float)z,
                        x2, (float)(z + stepZ), cellColor);
            addTriangle(x1 + stepX, (float)z, x2 + stepX, (float)(z + stepZ),
                        x2, (float)(z + stepZ), cellColor);
        }
    }
}

static void drawGround(float carZ)
{
    const int verticesPerRow = (80 / 4) * 6;
    // The chase camera is 18 units behind the car. Starting at carZ - 20
    // put one terrain strip through the eye plane; its clip-space W becomes
    // zero and real RSX expands the triangle into the screen-wide starburst.
    // Keep the nearest emitted row safely in front of the camera.
    const float firstVisibleZ = carZ - 6.0f;
    int start = (int)floorf((firstVisibleZ + 20.0f) / 4.0f) * verticesPerRow;
    if (start < 0) start = 0;
    int count = 30 * verticesPerRow;
    if (start >= (int)trackData.size()) return;
    if (start + count > (int)trackData.size()) count = (int)trackData.size() - start;

    tiny3d_SetPolygon(TINY3D_TRIANGLES);
    for (int i = 0; i < count; ++i) {
        const TerrainVertex &v = trackData[start + i];
        tiny3d_VertexPos(v.x, v.y, v.z);
        tiny3d_VertexColor(v.color);
    }
    tiny3d_End();
}

static MATRIX transform(float x, float y, float z, float yaw, float pitch,
                        float roll, float sx, float sy, float sz)
{
    MATRIX result = MatrixScale(sx, sy, sz);
    result = MatrixMultiply(result, MatrixRotationX(pitch));
    result = MatrixMultiply(result, MatrixRotationY(yaw));
    result = MatrixMultiply(result, MatrixRotationZ(roll));
    result = MatrixMultiply(result, MatrixTranslation(x, y, z));
    return result;
}

static void vertex(float x, float y, float z, u32 color)
{
    tiny3d_VertexPos(x, y, z);
    tiny3d_VertexColor(color);
}

// Superseded by drawCarModel(), which renders the packaged OBJ mesh.
#if 0
static void drawCube(const MATRIX &view, const MATRIX &model, u32 top,
                     u32 side, u32 bottom)
{
    // tiny3d matrices use row vectors: local vertex * model * view.
    // Reversing this order puts the model in camera space and sends it
    // outside the visible frustum as soon as the camera moves.
    const MATRIX mv = MatrixMultiply(model, view);
    tiny3d_SetMatrixModelView((MATRIX *)&mv);
    tiny3d_SetPolygon(TINY3D_QUADS);

    vertex(-1,  1, -1, top); vertex( 1,  1, -1, top);
    vertex( 1,  1,  1, top); vertex(-1,  1,  1, top);
    vertex(-1, -1, -1, bottom); vertex(-1, -1,  1, bottom);
    vertex( 1, -1,  1, bottom); vertex( 1, -1, -1, bottom);
    vertex(-1, -1,  1, side); vertex(-1,  1,  1, side);
    vertex( 1,  1,  1, side); vertex( 1, -1,  1, side);
    vertex( 1, -1, -1, side); vertex( 1,  1, -1, side);
    vertex(-1,  1, -1, side); vertex(-1, -1, -1, side);
    vertex(-1, -1, -1, side); vertex( 1, -1, -1, side);
    vertex( 1,  1, -1, side); vertex(-1,  1, -1, side);
    tiny3d_End();
}

static void drawCar(const MATRIX &view, const Car &car, bool left, bool right)
{
    const float yaw = car.angle * DEG2RAD;
    const MATRIX body = transform(car.x, car.y + 0.8f, car.z, yaw,
                                  car.pitch * DEG2RAD, car.roll * DEG2RAD,
                                  0.9f, 0.6f, 2.0f);
    drawCube(view, body, rgba(235, 235, 235), rgba(195, 28, 28), rgba(45, 45, 50));

    const float steer = left ? 30.0f : (right ? -30.0f : 0.0f);
    const float wheelX[4] = {-0.85f, 0.85f, -0.85f, 0.85f};
    const float wheelZ[4] = {-1.45f, -1.45f, 1.45f, 1.45f};
    for (int i = 0; i < 4; ++i) {
        const float wheelYaw = (i < 2 ? steer : 0.0f) * DEG2RAD;
        MATRIX wheel = transform(car.x, car.y + 0.25f, car.z, yaw,
                                 0.0f, 0.0f, 0.20f, 0.38f, 0.48f);
        MATRIX local = MatrixTranslation(wheelX[i], -0.2f, wheelZ[i]);
        local = MatrixMultiply(local, MatrixRotationY(wheelYaw));
        wheel = MatrixMultiply(wheel, local);
        drawCube(view, wheel, rgba(35, 35, 38), rgba(18, 18, 20), rgba(12, 12, 14));
    }
}

#endif

static void drawCarModel(const MATRIX &view, const Car &car)
{
    if (carMesh.vertices == NULL || carMesh.vertexCount == 0) return;

    const float yaw = car.angle * DEG2RAD;
    const float c = cosf(yaw);
    const float s = sinf(yaw);
    // Emit world-space vertices and reuse the exact view matrix used by the
    // road. This bypasses the ambiguous model/view multiplication entirely.
    tiny3d_SetMatrixModelView((MATRIX *)&view);
    tiny3d_SetPolygon(TINY3D_TRIANGLES);

    for (unsigned int i = 0; i < carMesh.vertexCount; ++i) {
        const ObjVertex &v = carMesh.vertices[i];
        const u32 color = (i / 3) % 2 == 0 ? rgba(206, 36, 30) : rgba(238, 55, 40);
        const float localX = v.x * CAR_MESH_SCALE;
        const float localY = v.y * CAR_MESH_SCALE;
        const float localZ = v.z * CAR_MESH_SCALE;
        const float worldX = car.x + localX * c + localZ * s;
        const float worldZ = car.z - localX * s + localZ * c;
        vertex(worldX, car.y + CAR_RIDE_HEIGHT + localY, worldZ, color);
    }
    tiny3d_End();
}

static void drawMissingCarMarker(const MATRIX &view, const Car &car)
{
    if (carMeshLoaded) return;
    tiny3d_SetMatrixModelView((MATRIX *)&view);
    tiny3d_SetPolygon(TINY3D_TRIANGLES);
    const u32 color = rgba(255, 0, 255);
    vertex(car.x - 1.5f, car.y, car.z, color);
    vertex(car.x + 1.5f, car.y, car.z, color);
    vertex(car.x, car.y + 3.0f, car.z, color);
    tiny3d_End();
}

static void emitDust(float x, float y, float z, u8 r, u8 g, u8 b)
{
    for (unsigned i = 0; i < sizeof(particles) / sizeof(particles[0]); ++i) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].x = x + ((rand() % 100) / 100.0f - 0.5f);
            particles[i].y = y;
            particles[i].z = z + ((rand() % 100) / 100.0f - 0.5f);
            particles[i].vx = ((rand() % 100) / 50.0f - 1.0f) * 3.0f;
            particles[i].vy = ((rand() % 100) / 100.0f) * 4.0f + 1.0f;
            particles[i].vz = ((rand() % 100) / 50.0f - 1.0f) * 3.0f;
            particles[i].maxLife = 0.5f + ((rand() % 100) / 100.0f) * 0.5f;
            particles[i].life = particles[i].maxLife;
            particles[i].size = 0.3f;
            particles[i].r = r; particles[i].g = g; particles[i].b = b;
            return;
        }
    }
}

static void updateParticles(float dt)
{
    for (unsigned i = 0; i < sizeof(particles) / sizeof(particles[0]); ++i) {
        if (!particles[i].active) continue;
        particles[i].x += particles[i].vx * dt;
        particles[i].y += particles[i].vy * dt;
        particles[i].z += particles[i].vz * dt;
        particles[i].size += 3.0f * dt;
        particles[i].life -= dt;
        if (particles[i].life <= 0.0f) particles[i].active = false;
    }
}

static void drawParticles(const MATRIX &view, const VECTOR &billboardRight,
                          const VECTOR &billboardUp)
{
    tiny3d_BlendFunc(1,
        (blend_src_func)(TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA),
        (blend_dst_func)(TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO),
        (blend_func)(TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD));
    tiny3d_SetMatrixModelView((MATRIX *)&view);

    for (unsigned i = 0; i < sizeof(particles) / sizeof(particles[0]); ++i) {
        if (!particles[i].active) continue;
        const float alpha = particles[i].life / particles[i].maxLife;
        const float rx = billboardRight.x * particles[i].size;
        const float ry = billboardRight.y * particles[i].size;
        const float rz = billboardRight.z * particles[i].size;
        const float ux = billboardUp.x * particles[i].size;
        const float uy = billboardUp.y * particles[i].size;
        const float uz = billboardUp.z * particles[i].size;
        const float x = particles[i].x;
        const float y = particles[i].y;
        const float z = particles[i].z;
        tiny3d_SetPolygon(TINY3D_QUADS);
        const u32 c = rgba(particles[i].r, particles[i].g, particles[i].b,
                           (u8)(alpha * 150.0f));
        vertex(x - rx - ux, y - ry - uy, z - rz - uz, c);
        vertex(x + rx - ux, y + ry - uy, z + rz - uz, c);
        vertex(x + rx + ux, y + ry + uy, z + rz + uz, c);
        vertex(x - rx + ux, y - ry + uy, z - rz + uz, c);
        tiny3d_End();
    }
    tiny3d_BlendFunc(0,
        TINY3D_BLEND_FUNC_SRC_RGB_ONE, TINY3D_BLEND_FUNC_DST_RGB_ZERO,
        TINY3D_BLEND_RGB_FUNC_ADD);
}

static void begin3D(const MATRIX &projection, const MATRIX &view)
{
    // This tiny3d release has no public tiny3d_EnableZTest() symbol.
    // tiny3d_Project3D() is its public equivalent and emits
    // rsxtiny_DepthTestEnable(1), ZControl and LESSOREQUAL.
    tiny3d_Project3D();
    tiny3d_SetProjectionMatrix((MATRIX *)&projection);
    tiny3d_SetMatrixModelView((MATRIX *)&view);
    tiny3d_AlphaTest(0, 0, TINY3D_ALPHA_FUNC_ALWAYS);
    tiny3d_BlendFunc(0,
        TINY3D_BLEND_FUNC_SRC_RGB_ONE, TINY3D_BLEND_FUNC_DST_RGB_ZERO,
        TINY3D_BLEND_RGB_FUNC_ADD);
}

static void drawHud(float speed, int surface, bool meshLoaded)
{
    tiny3d_Project2D();
    tiny3d_SetProjectionMatrix(NULL);
    tiny3d_SetMatrixModelView(NULL);

    const float amount = fminf(fabsf(speed) / 55.0f, 1.0f);
    tiny3d_SetPolygon(TINY3D_QUADS);
    vertex(20.0f, 20.0f, 1.0f, rgba(10, 12, 18, 210));
    vertex(300.0f, 20.0f, 1.0f, rgba(10, 12, 18, 210));
    vertex(300.0f, 42.0f, 1.0f, rgba(10, 12, 18, 210));
    vertex(20.0f, 42.0f, 1.0f, rgba(10, 12, 18, 210));
    tiny3d_End();

    // A tiny status swatch makes asset loading observable without relying on
    // the TTY log: green means the packaged OBJ was parsed; magenta means the
    // fallback marker in the 3D world should be visible instead.
    const u32 meshStatus = meshLoaded ? rgba(50, 235, 95) : rgba(255, 0, 255);
    tiny3d_SetPolygon(TINY3D_QUADS);
    vertex(306.0f, 20.0f, 1.0f, meshStatus);
    vertex(318.0f, 20.0f, 1.0f, meshStatus);
    vertex(318.0f, 32.0f, 1.0f, meshStatus);
    vertex(306.0f, 32.0f, 1.0f, meshStatus);
    tiny3d_End();

    const u32 barColor = surface == 0 ? rgba(225, 225, 225)
                       : (surface == 1 ? rgba(205, 145, 65) : rgba(55, 170, 65));
    tiny3d_SetPolygon(TINY3D_QUADS);
    vertex(25.0f, 25.0f, 0.0f, barColor);
    vertex(25.0f + 265.0f * amount, 25.0f, 0.0f, barColor);
    vertex(25.0f + 265.0f * amount, 37.0f, 0.0f, barColor);
    vertex(25.0f, 37.0f, 0.0f, barColor);
    tiny3d_End();
}

static void readPad(bool &left, bool &right, bool &up, bool &down)
{
    left = right = up = down = false;
    padInfo info = {};
    padData data = {};
    // PSL1GHT documents that the structure is zero-filled when no input has
    // changed. Do not accept a short/partial packet as button state.
    if (ioPadGetInfo(&info) == 0 && info.status[0] &&
        ioPadGetData(0, &data) == 0 && data.len >= 8) {
        left = data.BTN_LEFT != 0;
        right = data.BTN_RIGHT != 0;
        // Cross/Circle are deliberately the only pedals. This prevents an
        // emulator's stale D-pad Up state from acting as a throttle.
        up = data.BTN_CROSS != 0;
        down = data.BTN_CIRCLE != 0;
    }
}

static bool loadCarModel()
{
    // On a packaged homebrew title, the current directory is USRDIR itself.
    // The real console does not expose RPCS3's host-style /app_home mapping,
    // so try that proven package-local name before optional emulator paths.
    const char *paths[] = {
        "car.obj",
        "./car.obj",
        "/app_home/car.obj",
        "/app_home/Car.obj",
        "/dev_hdd0/game/RALLY0001/USRDIR/car.obj",
        "/dev_hdd0/game/RALLY0001/USRDIR/Car.obj"
    };

    for (unsigned int i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (objLoad(&carMesh, paths[i])) return true;
    }
    return false;
}

int main(int, char **)
{
    // SDL2 from ps3toolchain is used only for its portable timer. tiny3d owns
    // the RSX/video initialization, as required by the PSL1GHT backend.
    const bool sdlReady = SDL_Init(SDL_INIT_TIMER) == 0;
    if (tiny3d_Init(TINY3D_Z16 | 4 * 1024 * 1024) < 0) {
        fprintf(stderr, "tiny3d init failed\n");
        if (sdlReady) SDL_Quit();
        return 1;
    }
    
    ioPadInit(7);
    // Discard any input sampled while RPCS3 is attaching its virtual pad.
    // Without this, an old press can be applied on the first simulation tick.
    ioPadClearBuf(0);
    srand(1);
    generateLevel();
    carMeshLoaded = loadCarModel();
    if (!carMeshLoaded) {
        // Do not terminate the whole game because an external asset was
        // misplaced. The renderer will skip the car and TTY will show this.
        fprintf(stderr, "Unable to load car.obj from USRDIR\n");
    }

    Car car;
    car.x = trackCenterX(0.0f);
    car.y = terrainHeight(car.x, car.z) + CAR_RIDE_HEIGHT;
    const float dx = trackCenterX(5.0f) - car.x;
    car.angle = 180.0f + atan2f(dx, 5.0f) / DEG2RAD;
    u64 lastTime = sdlReady ? (u64)SDL_GetTicks() * 1000ULL : systemTimeMicros();

    while (true) {
        const u64 now = sdlReady ? (u64)SDL_GetTicks() * 1000ULL : systemTimeMicros();
        float dt = (float)(now - lastTime) / 1000000.0f;
        lastTime = now;
        if (dt < 0.0f || dt > 0.05f) dt = 1.0f / 60.0f;

        bool left, right, up, down;
        readPad(left, right, up, down);

        const int surface = surfaceType(car.x, car.z);
        if (fabsf(car.speed) > 0.5f) {
            const float dir = car.speed > 0.0f ? 1.0f : -1.0f;
            if (left) car.angle += car.turnSpeed * dt * dir;
            if (right) car.angle -= car.turnSpeed * dt * dir;
        }

        const float rad = car.angle * DEG2RAD;
        const float dirX = -sinf(rad);
        const float dirZ = -cosf(rad);
        float accel = car.acceleration;
        float friction = car.frictionAsphalt;
        float grip = 6.0f * dt;
        if (surface == 1) { accel *= 0.8f; friction = 2.0f; grip = 2.0f * dt; }
        if (surface == 2) { accel *= 0.4f; friction = car.frictionGrass; grip = 1.0f * dt; }

        bool pedal = false;
        if (up) { car.speed += accel * dt; pedal = true; }
        if (down) { car.speed -= accel * 0.5f * dt; pedal = true; }
        if (!pedal) {
            car.speed -= car.speed * friction * dt;
            if (fabsf(car.speed) < 0.1f) car.speed = 0.0f;
        }
        if (!(car.speed == car.speed)) car.speed = 0.0f;
        if (car.speed > car.maxSpeed) car.speed = car.maxSpeed;
        if (car.speed < -car.maxSpeed * 0.5f) car.speed = -car.maxSpeed * 0.5f;

        float groundY = terrainHeight(car.x, car.z);
        car.velY -= 40.0f * dt;
        car.y += car.velY * dt;
        if (car.y <= groundY + CAR_RIDE_HEIGHT) {
            car.y = groundY + CAR_RIDE_HEIGHT;
            car.velY = 0.0f;
        }
        else grip *= 0.1f;
        car.velX += (dirX * car.speed - car.velX) * grip;
        car.velZ += (dirZ * car.speed - car.velZ) * grip;
        const float horizontalSpeed = sqrtf(car.velX * car.velX + car.velZ * car.velZ);
        if (horizontalSpeed > car.maxSpeed) {
            const float velocityScale = car.maxSpeed / horizontalSpeed;
            car.velX *= velocityScale;
            car.velZ *= velocityScale;
        }
        car.x += car.velX * dt;
        car.z += car.velZ * dt;

        // Resolve collision against the terrain under the *new* horizontal
        // position. The old code tested only the previous cell, allowing the
        // vehicle to sink through an uphill road segment for one or more frames.
        groundY = terrainHeight(car.x, car.z);
        if (car.y <= groundY + CAR_RIDE_HEIGHT) {
            car.y = groundY + CAR_RIDE_HEIGHT;
            car.velY = 0.0f;
        }

        const float currentSpeed = sqrtf(car.velX * car.velX + car.velZ * car.velZ);
        if (currentSpeed > 10.0f && car.y <= groundY + CAR_RIDE_HEIGHT + 0.1f) {
            const float nx = car.velX / currentSpeed;
            const float nz = car.velZ / currentSpeed;
            const float dot = nx * dirX + nz * dirZ;
            if ((surface == 0 && dot < 0.95f) || surface != 0) {
                const u8 r = surface == 0 ? 180 : (surface == 1 ? 180 : 90);
                const u8 g = surface == 0 ? 180 : (surface == 1 ? 150 : 65);
                const u8 b = surface == 0 ? 180 : (surface == 1 ? 95 : 38);
                const float rightX = cosf(rad), rightZ = -sinf(rad);
                const float rearX = car.x + dirX * 1.5f;
                const float rearZ = car.z + dirZ * 1.5f;
                for (int i = 0; i < (surface == 1 ? 4 : 2); ++i) {
                    emitDust(rearX - rightX * 0.8f, car.y, rearZ - rightZ * 0.8f, r, g, b);
                    emitDust(rearX + rightX * 0.8f, car.y, rearZ + rightZ * 0.8f, r, g, b);
                }
            }
        }
        const float noseY = terrainHeight(car.x + dirX * 2.0f, car.z + dirZ * 2.0f);
        const float sideY = terrainHeight(car.x - dirZ * 1.5f, car.z + dirX * 1.5f);
        float targetPitch = (noseY - groundY) * 30.0f;
        float targetRoll = (sideY - groundY) * 30.0f;
        if (left) targetRoll -= fabsf(car.speed) * 0.3f;
        if (right) targetRoll += fabsf(car.speed) * 0.3f;
        car.pitch += (targetPitch - car.pitch) * 5.0f * dt;
        car.roll += (targetRoll - car.roll) * 5.0f * dt;
        updateParticles(dt);

        // tiny3d's stock projection expects visible geometry at +Z (see its
        // spheres3D sample, which uses MatrixTranslation(..., 80)). Build the
        // chase view in that convention instead of MakeLookAt's -Z convention.
        // A 45-degree lens and a target close to the car remove the extreme
        // foreground expansion from the previous low, 60-degree chase view.
        // The car remains inside the centre third of the screen.
        VECTOR eye = {car.x - dirX * 18.0f, car.y + 8.0f, car.z - dirZ * 18.0f};
        VECTOR target = {car.x + dirX * 1.5f, car.y + 0.8f, car.z + dirZ * 1.5f};
        const float cameraYaw = PI - rad;
        const float cameraPitch = -atan2f(eye.y - target.y, 19.5f);
        MATRIX view = MatrixTranslation(-eye.x, -eye.y, -eye.z);
        view = MatrixMultiply(view, MatrixRotationY(cameraYaw));
        view = MatrixMultiply(view, MatrixRotationX(cameraPitch));
        // This is the aspect convention used by tiny3d's own spheres3D
        // sample. The renderer's 3D viewport already supplies the physical
        // 16:9 scaling; MatrixProjPerspective therefore receives 9/16.
        const float aspect = Video_aspect == 1 ? 9.0f / 16.0f : 1.0f;
        MATRIX projection = MatrixProjPerspective(45.0f, aspect, 0.25f, 300.0f);

        // Build a camera-facing basis for the dust quads. The camera can
        // pitch slightly, so derive it from the actual look direction
        // instead of rotating a square around world Y only.
        float fx = target.x - eye.x;
        float fy = target.y - eye.y;
        float fz = target.z - eye.z;
        const float forwardLength = sqrtf(fx * fx + fy * fy + fz * fz);
        fx /= forwardLength;
        fy /= forwardLength;
        fz /= forwardLength;
        float rx = fz;
        float rz = -fx;
        const float rightLength = sqrtf(rx * rx + rz * rz);
        rx /= rightLength;
        rz /= rightLength;
        VECTOR billboardRight = {rx, 0.0f, rz};
        VECTOR billboardUp = {-rz * fy, rz * fx - rx * fz, rx * fy};

        tiny3d_Clear(rgba(100, 155, 220), TINY3D_CLEAR_ALL);
        tiny3d_SetLightsOff();
        begin3D(projection, view);
        drawGround(car.z);
        begin3D(projection, view);
        drawCarModel(view, car);
        drawMissingCarMarker(view, car);
        begin3D(projection, view);
        drawParticles(view, billboardRight, billboardUp);
        drawHud(car.speed, surface, carMeshLoaded);
        tiny3d_Flip();
    }

    ioPadEnd();
    if (sdlReady) SDL_Quit();
    return 0;
}
