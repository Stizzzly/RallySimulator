#include <vectormath/cpp/vectormath_aos.h>

#include <EGL/egl.h>
#define GL3_PROTOTYPES
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>
#include <GL3/rsxgl.h>
#include <GL3/rsxgl3ext.h>
#include <io/pad.h>
#include <sys/process.h>
#include <sys/systime.h>
#include <sysutil/sysutil.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PPU/RSX headers expose Altivec's `vector` keyword as a macro. */
#ifdef vector
#undef vector
#endif
#include <vector>

#include "rsxgl_config.h"
#include "rally_vert.h"
#include "rally_frag.h"

using namespace Vectormath::Aos;

SYS_PROCESS_PARAM(1001, 0x100000);

static const float PI = 3.14159265358979323846f;
static const float ROAD_HALF_WIDTH = 10.0f;
static const float CURB_WIDTH = 1.5f;
static const int LEVEL_LENGTH = 4000;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

struct Position {
    float x, y, z;
};

struct Car {
    float x, y, z;
    float velX, velY, velZ;
    float angle, pitch, roll, speed;
    float acceleration, maxSpeed, turnSpeed;
    float frictionAsphalt, frictionGrass;
};

struct Controls {
    float steer;
    bool throttle;
    bool brake;
};

static int running = 1;
static int drawing = 1;
static int width = 0;
static int height = 0;
static GLuint program = 0;
static GLuint terrainBuffer = 0;
static GLuint carBuffer = 0;
static GLsizei terrainCount = 0;
static GLsizei carCount = 0;
static int terrainWindowRow = -1;
static GLint positionLocation = -1;
static GLint colorLocation = -1;
static GLint projectionLocation = -1;
static GLint viewLocation = -1;
static Matrix4 projectionMatrix;

static float trackCenterX(float z)
{
    return sinf(z * 0.015f) * 40.0f + sinf(z * 0.04f) * 20.0f + cosf(z * 0.08f) * 10.0f;
}

static void eventHandler(u64 status, u64, void *)
{
    if (status == SYSUTIL_EXIT_GAME) running = 0;
    if (status == SYSUTIL_MENU_OPEN) drawing = 0;
    if (status == SYSUTIL_MENU_CLOSE) drawing = 1;
}

static float terrainHeight(float x, float z)
{
    const float distance = fabsf(x - trackCenterX(z));
    const float base = sinf(z * 0.03f) * 3.0f;
    if (distance < ROAD_HALF_WIDTH + CURB_WIDTH) return base + 0.2f;
    if (distance < ROAD_HALF_WIDTH + CURB_WIDTH + 25.0f)
        return base + 0.2f + sinf(x * 0.4f) * 0.3f;
    const float wall = distance - (ROAD_HALF_WIDTH + CURB_WIDTH + 25.0f);
    return base + 0.2f + wall * wall * 0.05f + sinf(x) * 0.5f;
}

static int surfaceType(float x, float z)
{
    const float distance = fabsf(x - trackCenterX(z));
    if (distance > ROAD_HALF_WIDTH + 0.5f) return 2;
    return sinf(z * 0.015f) + cosf(z * 0.045f) > 0.8f ? 1 : 0;
}

static void terrainColour(float x, float z, float &r, float &g, float &b)
{
    const float distance = fabsf(x - trackCenterX(z));
    const int surface = surfaceType(x, z);
    if (surface == 0) {
        const float grey = 0.25f + sinf(x * 10.0f) * 0.02f;
        if (distance < 0.4f && ((int)z / 4) % 2 == 0) r = g = b = 1.0f;
        else { r = grey; g = grey; b = grey + 0.05f; }
    } else if (surface == 1) {
        const float noise = sinf(x * 5.0f) * sinf(z * 5.0f) * 0.05f;
        r = 0.6f + noise; g = 0.5f + noise; b = 0.4f + noise;
    } else if (distance < ROAD_HALF_WIDTH + CURB_WIDTH) {
        if (((int)z / 3) % 2 == 0) { r = 0.9f; g = 0.1f; b = 0.1f; }
        else r = g = b = 0.9f;
    } else if (((int)x / 4 + (int)z / 4) % 2 == 0) {
        r = 0.1f; g = 0.4f; b = 0.1f;
    } else { r = 0.2f; g = 0.5f; b = 0.2f; }
}

static void appendVertex(std::vector<Vertex> &out, float x, float y, float z,
                         float r, float g, float b)
{
    Vertex v = {x, y, z, r, g, b, 1.0f};
    out.push_back(v);
}

static void appendQuad(std::vector<Vertex> &out, float x0, float z0,
                       float x1, float z1, float r, float g, float b)
{
    const float y00 = terrainHeight(x0, z0);
    const float y10 = terrainHeight(x1, z0);
    const float y01 = terrainHeight(x0, z1);
    const float y11 = terrainHeight(x1, z1);
    appendVertex(out, x0, y00, z0, r, g, b);
    appendVertex(out, x1, y10, z0, r, g, b);
    appendVertex(out, x1, y11, z1, r, g, b);
    appendVertex(out, x0, y00, z0, r, g, b);
    appendVertex(out, x1, y11, z1, r, g, b);
    appendVertex(out, x0, y01, z1, r, g, b);
}

static void buildTerrain()
{
    glGenBuffers(1, &terrainBuffer);
}

/* RSXGL is an old OpenGL implementation.  Keep only the same 30 visible
   rows the original game drew, rather than a 4 km VBO in RSX memory. */
static void updateTerrainWindow(float carZ)
{
    const int verticesPerRow = 20 * 6;
    int currentRow = (int)((carZ + 20.0f) / 4.0f);
    int firstRow = currentRow - 5;
    if (firstRow < 0) firstRow = 0;
    if (firstRow == terrainWindowRow) return;
    std::vector<Vertex> vertices;
    const float cell = 4.0f;
    vertices.reserve(30 * verticesPerRow);
    for (int row = firstRow; row < firstRow + 30; ++row) {
        const float z = -20.0f + row * cell;
        if (z >= LEVEL_LENGTH) break;
        const float center0 = trackCenterX(z);
        const float center1 = trackCenterX(z + cell);
        for (float localX = -40.0f; localX < 40.0f; localX += cell) {
            const float x00 = center0 + localX;
            const float x10 = center0 + localX + cell;
            const float x01 = center1 + localX;
            const float x11 = center1 + localX + cell;
            float r, g, b;
            terrainColour((x00 + x10 + x01 + x11) * 0.25f, z + cell * 0.5f, r, g, b);
            const float y00 = terrainHeight(x00, z), y10 = terrainHeight(x10, z);
            const float y01 = terrainHeight(x01, z + cell), y11 = terrainHeight(x11, z + cell);
            appendVertex(vertices, x00, y00, z, r, g, b); appendVertex(vertices, x10, y10, z, r, g, b); appendVertex(vertices, x01, y01, z + cell, r, g, b);
            appendVertex(vertices, x10, y10, z, r, g, b); appendVertex(vertices, x11, y11, z + cell, r, g, b); appendVertex(vertices, x01, y01, z + cell, r, g, b);
        }
    }
    terrainCount = (GLsizei)vertices.size();
    glBindBuffer(GL_ARRAY_BUFFER, terrainBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), &vertices[0], GL_DYNAMIC_DRAW);
    terrainWindowRow = firstRow;
}

static bool loadCarMesh(std::vector<Vertex> &out)
{
    /* A retail console does not guarantee the process working directory.
       The installed package's absolute USRDIR path is therefore first. */
    const char *paths[] = {
        "/dev_hdd0/game/RALLYGL01/USRDIR/car.obj",
        "/dev_hdd0/game/RALLYGL01/USRDIR/Car.obj",
        "car.obj", "./car.obj", "/app_home/car.obj"
    };
    FILE *file = NULL;
    for (unsigned int i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        file = fopen(paths[i], "r");
        if (file != NULL) {
            printf("Loaded car model from %s\n", paths[i]);
            break;
        }
    }
    if (file == NULL) {
        printf("ERROR: car.obj was not found\n");
        return false;
    }

    std::vector<Position> positions;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        Position p;
        int a, b, c;
        if (sscanf(line, "v %f %f %f", &p.x, &p.y, &p.z) == 3) {
            positions.push_back(p);
        } else if (sscanf(line, "f %d %d %d", &a, &b, &c) == 3) {
            const int index[3] = {a - 1, b - 1, c - 1};
            for (int i = 0; i < 3; ++i) {
                if (index[i] < 0 || index[i] >= (int)positions.size()) continue;
                const Position &v = positions[index[i]];
                appendVertex(out, v.x, v.y, v.z, 0.92f, 0.08f, 0.04f);
            }
        }
    }
    fclose(file);
    printf("Car mesh: %u triangles\n", (unsigned int)(out.size() / 3));
    return !out.empty();
}

static void buildCar()
{
    std::vector<Vertex> vertices;
    if (!loadCarMesh(vertices)) {
        /* Keep a visible diagnostic vehicle even if an asset path is wrong. */
        const float p[8][3] = {
            {-0.9f, 0.0f, -1.8f}, {0.9f, 0.0f, -1.8f},
            {0.9f, 0.0f, 1.8f}, {-0.9f, 0.0f, 1.8f},
            {-0.9f, 0.8f, -1.8f}, {0.9f, 0.8f, -1.8f},
            {0.9f, 0.8f, 1.8f}, {-0.9f, 0.8f, 1.8f}
        };
        const int triangles[36] = {0,1,2, 0,2,3, 4,6,5, 4,7,6,
                                   0,4,5, 0,5,1, 1,5,6, 1,6,2,
                                   2,6,7, 2,7,3, 3,7,4, 3,4,0};
        for (int i = 0; i < 36; ++i) {
            const float *v = p[triangles[i]];
            appendVertex(vertices, v[0], v[1], v[2], 1.0f, 0.0f, 1.0f);
        }
        printf("Using magenta fallback car mesh\n");
    }
    carCount = (GLsizei)vertices.size();
    glGenBuffers(1, &carBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, carBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
}

static void bindMesh(GLuint buffer)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(positionLocation);
    glEnableVertexAttribArray(colorLocation);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    glVertexAttribPointer(colorLocation, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (const GLvoid *)(sizeof(float) * 3));
}

static void initRenderer()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    const float degreesToRadians = PI / 180.0f;
    projectionMatrix = Matrix4::perspective(48.0f * degreesToRadians,
                                            (float)width / (float)height,
                                            0.25f, 500.0f);
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *sources[] = {(const GLchar *)rally_vert, (const GLchar *)rally_frag};
    const GLint lengths[] = {(GLint)rally_vert_len, (GLint)rally_frag_len};
    glShaderSource(vertexShader, 1, sources, lengths);
    glCompileShader(vertexShader);
    glShaderSource(fragmentShader, 1, sources + 1, lengths + 1);
    glCompileShader(fragmentShader);
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glUseProgram(program);
    positionLocation = glGetAttribLocation(program, "position");
    colorLocation = glGetAttribLocation(program, "color");
    projectionLocation = glGetUniformLocation(program, "Projection");
    viewLocation = glGetUniformLocation(program, "View");
    printf("RSXGL attributes position=%d color=%d; uniforms Projection=%d View=%d\n",
           positionLocation, colorLocation, projectionLocation, viewLocation);
    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, (const GLfloat *)&projectionMatrix);
    buildTerrain();
    updateTerrainWindow(0.0f);
    buildCar();
}

static u64 nowMicros()
{
    u64 seconds = 0, nanoseconds = 0;
    sysGetCurrentTime(&seconds, &nanoseconds);
    return seconds * 1000000ULL + nanoseconds / 1000ULL;
}

static Controls readControls()
{
    padInfo info = {};
    padData data = {};
    Controls controls = {0.0f, false, false};
    if (ioPadGetInfo(&info) == 0) {
        /* This is the simple polling path used by the known-good cube sample.
           Some real pads report len=0 while a button is held, so do not gate
           state on packet length. */
        for (int i = 0; i < MAX_PADS; ++i) {
            if (!info.status[i] || ioPadGetData(i, &data) != 0) continue;
            controls.steer = (data.BTN_LEFT ? 1.0f : 0.0f) - (data.BTN_RIGHT ? 1.0f : 0.0f);
            controls.throttle = data.BTN_UP || data.BTN_CROSS || data.BTN_R2;
            controls.brake = data.BTN_DOWN || data.BTN_CIRCLE || data.BTN_L2;
            const float stick = ((float)data.ANA_L_H - 128.0f) / 127.0f;
            if (fabsf(stick) > 0.16f) controls.steer = -stick;
            if (data.ANA_L_V < 96) controls.throttle = true;
            if (data.ANA_L_V > 160) controls.brake = true;
            break;
        }
    }
    return controls;
}

static void updateCar(Car &car, const Controls &controls, float dt)
{
    const int surface = surfaceType(car.x, car.z);
    if (fabsf(car.speed) > 0.5f) {
        const float direction = car.speed > 0.0f ? 1.0f : -1.0f;
        car.angle += controls.steer * car.turnSpeed * dt * direction;
    }
    const float rad = car.angle * PI / 180.0f;
    const float forwardX = -sinf(rad), forwardZ = -cosf(rad);
    float acceleration = car.acceleration, friction = car.frictionAsphalt, grip = 6.0f * dt;
    if (surface == 1) { acceleration *= 0.8f; friction = 2.0f; grip = 2.0f * dt; }
    if (surface == 2) { acceleration *= 0.4f; friction = car.frictionGrass; grip = 1.0f * dt; }
    if (controls.throttle) car.speed += acceleration * dt;
    if (controls.brake) car.speed -= acceleration * 0.5f * dt;
    if (!controls.throttle && !controls.brake) {
        car.speed -= car.speed * friction * dt;
        if (fabsf(car.speed) < 0.1f) car.speed = 0.0f;
    }
    if (car.speed > car.maxSpeed) car.speed = car.maxSpeed;
    if (car.speed < -car.maxSpeed * 0.5f) car.speed = -car.maxSpeed * 0.5f;
    float ground = terrainHeight(car.x, car.z);
    car.velY -= 40.0f * dt;
    car.y += car.velY * dt;
    if (car.y <= ground) { car.y = ground; car.velY = 0.0f; } else grip *= 0.1f;
    car.velX += (forwardX * car.speed - car.velX) * grip;
    car.velZ += (forwardZ * car.speed - car.velZ) * grip;
    car.x += car.velX * dt;
    car.z += car.velZ * dt;
    ground = terrainHeight(car.x, car.z);
    if (car.y <= ground) { car.y = ground; car.velY = 0.0f; }
    const float nose = terrainHeight(car.x + forwardX * 2.0f, car.z + forwardZ * 2.0f);
    const float side = terrainHeight(car.x - forwardZ * 1.5f, car.z + forwardX * 1.5f);
    float targetPitch = (nose - ground) * 30.0f;
    float targetRoll = (side - ground) * 30.0f + controls.steer * fabsf(car.speed) * -0.3f;
    car.pitch += (targetPitch - car.pitch) * 5.0f * dt;
    car.roll += (targetRoll - car.roll) * 5.0f * dt;
}

static void drawFrame(const Car &car, float &camX, float &camY, float &camZ, float &camAngle, float dt)
{
    glClearColor(0.32f, 0.66f, 0.86f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camX += (car.x - camX) * 10.0f * dt;
    camY += (car.y - camY) * 10.0f * dt;
    camZ += (car.z - camZ) * 10.0f * dt;
    float difference = car.angle - camAngle;
    while (difference > 180.0f) difference -= 360.0f;
    while (difference < -180.0f) difference += 360.0f;
    camAngle += difference * 5.0f * dt;
    const float cameraRad = camAngle * PI / 180.0f;
    const Vector3 forward(-sinf(cameraRad), 0.0f, -cosf(cameraRad));
    const Point3 eye(camX - forward.getX() * 7.0f, camY + 2.5f, camZ - forward.getZ() * 7.0f);
    const Point3 target(eye + forward * 12.0f);
    const Matrix4 view = Matrix4::lookAt(eye, target, Vector3(0.0f, 1.0f, 0.0f));
    glUseProgram(program);
    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, (const GLfloat *)&view);
    updateTerrainWindow(car.z);
    bindMesh(terrainBuffer);
    if (terrainCount > 0) glDrawArrays(GL_TRIANGLES, 0, terrainCount);

    if (carCount > 0) {
        const Matrix4 model = Matrix4::translation(Vector3(car.x, car.y + 0.4f, car.z)) *
                              Matrix4::rotationY(car.angle * PI / 180.0f) *
                              Matrix4::rotationX(car.pitch * PI / 180.0f) *
                              Matrix4::rotationZ(car.roll * PI / 180.0f);
        const Matrix4 carView = view * model;
        glUniformMatrix4fv(viewLocation, 1, GL_FALSE, (const GLfloat *)&carView);
        bindMesh(carBuffer);
        glDrawArrays(GL_TRIANGLES, 0, carCount);
    }
}

int main()
{
    ioPadInit(7);
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL)) return 1;
    const EGLint attributes[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                 EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(display, attributes, &config, 1, &count) || count == 0) return 1;
    EGLSurface surface = eglCreateWindowSurface(display, config, 0, 0);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, 0);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(display, surface, surface, context)) return 1;
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    printf("RallySimulator RSXGL: %dx%d\n", width, height);
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, eventHandler, NULL);
    initRenderer();

    Car car = {};
    car.x = trackCenterX(0.0f);
    car.y = terrainHeight(car.x, 0.0f);
    car.z = 0.0f;
    car.angle = 180.0f + atan2f(trackCenterX(5.0f) - car.x, 5.0f) * 180.0f / PI;
    car.acceleration = 35.0f;
    car.maxSpeed = 55.0f;
    car.turnSpeed = 100.0f;
    car.frictionAsphalt = 1.5f;
    car.frictionGrass = 5.0f;
    float camX = car.x, camY = car.y, camZ = car.z, camAngle = car.angle;
    u64 previous = nowMicros();
    while (running) {
        const u64 current = nowMicros();
        float dt = (float)(current - previous) / 1000000.0f;
        previous = current;
        if (dt <= 0.0f || dt > 0.05f) dt = 1.0f / 60.0f;
        const Controls controls = readControls();
        updateCar(car, controls, dt);
        if (drawing) drawFrame(car, camX, camY, camZ, camAngle, dt);
        if (!eglSwapBuffers(display, surface)) break;
        sysUtilCheckCallback();
    }
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    eglDestroyContext(display, context);
    eglTerminate(display);
    ioPadEnd();
    return 0;
}
