#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/gl.h>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ctime>

// --- КОНСТАНТЫ ---
const int SCREEN_WIDTH = 960; // iPhone 4S resolution
const int SCREEN_HEIGHT = 640; // iPhone 4S resolution
const float DEG2RAD = 3.14159f / 180.0f;
const float ROAD_WIDTH = 10.0f;
const float CURB_WIDTH = 1.5f;

// --- СТРУКТУРЫ ДЛЯ МОДЕЛИ ---
struct Vector3 { float x, y, z; };
struct Face { int v1, v2, v3; };

// --- СТРУКТУРА МАШИНЫ ---
struct Car {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    float angle = 0.0f;
    float pitch = 0.0f, roll = 0.0f;

    // Характеристики
    float speed = 0.0f;
    float acceleration = 35.0f;
    float maxSpeed = 55.0f;
    float turnSpeed = 100.0f;
    float frictionAsphalt = 1.5f;
    float frictionGrass = 5.0f;
};

// --- ЗАГРУЗЧИК OBJ ---
bool loadOBJ(const char* path, std::vector<Vector3>& out_vertices, std::vector<Face>& out_faces) {
    std::ifstream file(path);
    if (!file.is_open()) {
        SDL_Log("ERROR: Could not open %s!", path);
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            Vector3 v;
            ss >> v.x >> v.y >> v.z;
            out_vertices.push_back(v);
        } else if (prefix == "f") {
            std::string s1, s2, s3;
            ss >> s1 >> s2 >> s3;

            // Парсим индексы (учитываем форматы v, v/vt, v//vn, v/vt/vn)
            auto getIdx = [](std::string& s) {
                size_t slash = s.find('/');
                if (slash != std::string::npos) s = s.substr(0, slash);
                return std::stoi(s) - 1; // OBJ starts at 1, C++ at 0
            };

            Face f;
            f.v1 = getIdx(s1);
            f.v2 = getIdx(s2);
            f.v3 = getIdx(s3);
            out_faces.push_back(f);
        }
    }
    SDL_Log("Loaded %s: %d vertices, %d faces", path, out_vertices.size(), out_faces.size());
    return true;
}

// --- ГЕНЕРАЦИЯ ТРАССЫ ---
float getTrackCenterX(float z) {
    return sinf(z * 0.015f) * 40.0f + sinf(z * 0.05f) * 15.0f;
}

float getTerrainHeight(float x, float z) {
    float centerX = getTrackCenterX(z);
    float dist = fabsf(x - centerX);
    if (dist < ROAD_WIDTH) return cosf(dist / ROAD_WIDTH) * 0.5f;
    if (dist < ROAD_WIDTH + CURB_WIDTH) return 0.2f;
    float wallDist = dist - (ROAD_WIDTH + CURB_WIDTH);
    return 0.2f + (wallDist * wallDist * 0.1f) + sinf(x)*0.5f;
}

void drawGround(float carZ) {
    int renderDist = 60;
    int step = 2;
    for (int z = (int)carZ - 10; z < (int)carZ + renderDist; z += step) {
        float centerX = getTrackCenterX((float)z);
        float leftLimit = centerX - 40.0f;
        float rightLimit = centerX + 40.0f;
        int xStart = (int)(leftLimit / step) * step;
        int xEnd = (int)(rightLimit / step) * step;

        glBegin(GL_TRIANGLE_STRIP);
        for (int x = xStart; x <= xEnd; x += step) {
            for (int k = 0; k <= 1; k++) {
                float curZ = (float)(z + (k * step));
                float curX = (float)x;
                float trkCenter = getTrackCenterX(curZ);
                float dist = fabsf(curX - trkCenter);
                float height = getTerrainHeight(curX, curZ);

                if (dist < ROAD_WIDTH) {
                    float grey = 0.25f + sinf(curX * 10.0f) * 0.02f;
                    if (dist < 0.4f && (int(curZ) / 4) % 2 == 0) glColor3f(1.0f, 1.0f, 1.0f);
                    else glColor3f(grey, grey, grey + 0.05f);
                } else if (dist < ROAD_WIDTH + CURB_WIDTH) {
                    if ((int(curZ) / 3) % 2 == 0) glColor3f(0.9f, 0.1f, 0.1f);
                    else glColor3f(0.9f, 0.9f, 0.9f);
                } else {
                    if ((int(curX)/4 + int(curZ)/4) % 2 == 0) glColor3f(0.1f, 0.4f, 0.1f);
                    else glColor3f(0.2f, 0.5f, 0.2f);
                }
                glVertex3f(curX, height, curZ);
            }
        }
        glEnd();
    }
}

// Фолбэк (старая коробка, если нет модели)
void drawBoxCar() {
    glPushMatrix();
    glScalef(0.9f, 0.6f, 2.0f);
    glBegin(GL_QUADS);
    glColor3f(0.8f, 0.0f, 0.0f);
    glVertex3f(-1, 1, -1); glVertex3f(1, 1, -1); glVertex3f(1, 1, 1); glVertex3f(-1, 1, 1);
    glColor3f(0.2f, 0.2f, 0.2f);
    glVertex3f(-1, 0, -1); glVertex3f(1, 0, -1); glVertex3f(1, 0, 1); glVertex3f(-1, 0, 1);
    glColor3f(0.6f, 0.0f, 0.0f);
    glVertex3f(-1, 0, 1); glVertex3f(1, 0, 1); glVertex3f(1, 1, 1); glVertex3f(-1, 1, 1);
    glVertex3f(-1, 0, -1); glVertex3f(1, 0, -1); glVertex3f(1, 1, -1); glVertex3f(-1, 1, -1);
    glVertex3f(-1, 0, -1); glVertex3f(-1, 0, 1); glVertex3f(-1, 1, 1); glVertex3f(-1, 1, -1);
    glVertex3f(1, 0, -1); glVertex3f(1, 0, 1); glVertex3f(1, 1, 1); glVertex3f(1, 1, -1);
    glEnd();
    glPopMatrix();
}

void setupPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    GLdouble xmin, xmax, ymin, ymax;
    ymax = zNear * tan(fovy * 3.14159265 / 360.0);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;
    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_Window* window = SDL_CreateWindow("Sega Rally Clone: First Demo",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_GLContext gContext = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    int swapInterval = SDL_GL_GetSwapInterval();
    SDL_Log("VSync swap interval: %d", swapInterval);
    SDL_Log("OpenGL Vendor: %s", (const char*)glGetString(GL_VENDOR));
    SDL_Log("OpenGL Renderer: %s", (const char*)glGetString(GL_RENDERER));
    SDL_Log("OpenGL Version: %s", (const char*)glGetString(GL_VERSION));
    SDL_Log("GLSL Version: %s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    GLfloat fogColor[] = {0.4f, 0.6f, 0.9f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 40.0f);
    glFogf(GL_FOG_END, 80.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setupPerspective(75.0, (double)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1, 150.0);

    // --- ЗАГРУЗКА МОДЕЛИ ---
    std::vector<Vector3> verts;
    std::vector<Face> faces;
    bool modelLoaded = loadOBJ("car.obj", verts, faces);

    GLuint carList = 0;
    if (modelLoaded) {
        carList = glGenLists(1);
        glNewList(carList, GL_COMPILE);
        glBegin(GL_TRIANGLES);
        glColor3f(0.9f, 0.9f, 0.9f); // Белый базовый цвет машины

        // Простая симуляция света (направленного сверху)
        for (const auto& f : faces) {
            // Берем координаты вершин
            Vector3 v1 = verts[f.v1];
            Vector3 v2 = verts[f.v2];
            Vector3 v3 = verts[f.v3];

            // Считаем нормаль (для освещения)
            float ux = v2.x - v1.x, uy = v2.y - v1.y, uz = v2.z - v1.z;
            float vx = v3.x - v1.x, vy = v3.y - v1.y, vz = v3.z - v1.z;
            float nx = uy * vz - uz * vy;
            float ny = uz * vx - ux * vz; // Y - вверх
            float nz = ux * vy - uy * vx;

            // Нормализуем
            float len = sqrt(nx*nx + ny*ny + nz*nz);
            if (len > 0) { nx /= len; ny /= len; nz /= len; }

            // Чем больше грань смотрит вверх (ny > 0), тем она светлее
            float brightness = 0.4f + (ny > 0 ? ny : 0) * 0.6f;

            // Если грань смотрит вбок - чуть темнее
            if (nx > 0.5 || nx < -0.5) brightness *= 0.8f;

            glColor3f(brightness, brightness, brightness);

            glVertex3f(v1.x, v1.y, v1.z);
            glVertex3f(v2.x, v2.y, v2.z);
            glVertex3f(v3.x, v3.y, v3.z);
        }
        glEnd();
        glEndList();
    }

    SDL_Log("Model loaded: %s, verts=%d, faces=%d, displayList=%u", modelLoaded?"yes":"no", (int)verts.size(), (int)faces.size(), carList);

    Car lada;
    lada.y = 1.0f;

    Uint32 lastTime = SDL_GetTicks();
    Uint32 fpsLastTime = lastTime;
    int fpsFrameCount = 0;
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    bool quit = false;
    SDL_Event e;

    while (!quit) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) quit = true;
        }

        // --- ФИЗИКА ---

        // Проверяем, на асфальте мы или нет (из твоего старого кода)
        float currentCenterX = getTrackCenterX(lada.z);
        float distFromCenter = fabsf(lada.x - currentCenterX);
        bool onAsphalt = (distFromCenter < ROAD_WIDTH + 1.0f);

        // 1. Поворот КУЗОВА (Аркадный)
        // Чтобы машина не крутилась на месте стоя, оставим проверку на минимальную скорость,
        // но сам поворот сделаем как ты просил.
        if (abs(lada.speed) > 0.5f) {
            // Определяем, едем мы вперед или назад, чтобы инвертировать руль при заднем ходе
            float dir = (lada.speed > 0) ? 1.0f : -1.0f;

        if (keys[SDL_SCANCODE_LEFT])  lada.angle += lada.turnSpeed * deltaTime * dir;
        if (keys[SDL_SCANCODE_RIGHT]) lada.angle -= lada.turnSpeed * deltaTime * dir;
    }

        // Вычисляем, куда СМОТРИТ капот машины прямо сейчас
        float rad = lada.angle * DEG2RAD;
        float targetDirX = -sin(rad);
        float targetDirZ = -cos(rad);

        // 2. Газ (Скорость)
        float currentAccel = lada.acceleration;
        if (!onAsphalt) currentAccel *= 0.5f; // Штраф к разгону на траве

        bool isPedalPressed = false;

        if (keys[SDL_SCANCODE_UP]) {
            lada.speed += currentAccel * deltaTime;
            isPedalPressed = true;
        } else if (keys[SDL_SCANCODE_DOWN]) {
            lada.speed -= (currentAccel * 0.5f) * deltaTime; // Тормоз/Задний ход
            isPedalPressed = true;
        }

        // Трение: работает, когда отпущены педали (машина идет накатом)
        if (!isPedalPressed) {
            float currentFriction = onAsphalt ? lada.frictionAsphalt : lada.frictionGrass;
            lada.speed -= lada.speed * currentFriction * deltaTime;

            // Полная остановка на сверхнизких скоростях, чтобы машина не "ползла" бесконечно
            if (abs(lada.speed) < 0.1f) lada.speed = 0.0f;
        }

        // Ограничитель скорости (используем твой maxSpeed)
        if (lada.speed > lada.maxSpeed) lada.speed = lada.maxSpeed;
        if (lada.speed < -lada.maxSpeed * 0.5f) lada.speed = -lada.maxSpeed * 0.5f; // Задний ход в 2 раза медленнее

// 3. МАГИЯ СЕГИ: Сцепление (Grip)
// Адаптировано под deltaTime, чтобы физика не зависела от FPS.
// 6.0f и 1.2f примерно равны твоим 0.1 и 0.02 при 60 кадрах в секунду.
// Можешь поиграть с этими числами: чем меньше число, тем сильнее дрифт!
float grip = onAsphalt ? (6.0f * deltaTime) : (1.2f * deltaTime);

// 4. Плавно "дотягиваем" реальный вектор скорости до вектора капота
lada.velX += (targetDirX * lada.speed - lada.velX) * grip;
lada.velZ += (targetDirZ * lada.speed - lada.velZ) * grip;

// 5. Двигаем машину по реальному вектору
lada.x += lada.velX * deltaTime;
lada.z += lada.velZ * deltaTime;


// --- ВИЗУАЛ И РЕЛЬЕФ (Твой старый код, адаптированный под новые векторы) ---

float targetY = getTerrainHeight(lada.x, lada.z);
lada.y = targetY;

float noseY = getTerrainHeight(lada.x + targetDirX * 2.0f, lada.z + targetDirZ * 2.0f);
float sideY = getTerrainHeight(lada.x - targetDirZ * 1.5f, lada.z + targetDirX * 1.5f);
float targetPitch = (noseY - targetY) * 30.0f;
float targetRoll = (sideY - targetY) * 30.0f;

// Наклон кузова при повороте (зависит от скорости машины)
if(keys[SDL_SCANCODE_LEFT])  targetRoll -= abs(lada.speed) * 0.3f;
if(keys[SDL_SCANCODE_RIGHT]) targetRoll += abs(lada.speed) * 0.3f;

        lada.pitch += (targetPitch - lada.pitch) * 5.0f * deltaTime;
        lada.roll += (targetRoll - lada.roll) * 5.0f * deltaTime;

        // --- РЕНДЕР ---
        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Камера
        glTranslatef(0.0f, -2.5f, -7.0f);
        glRotatef(-lada.angle, 0.0f, 1.0f, 0.0f);
        glTranslatef(-lada.x, -lada.y, -lada.z);

        // Рисуем машину
        glPushMatrix();
            glTranslatef(lada.x, lada.y + 0.4f, lada.z);
            glRotatef(lada.angle, 0.0f, 1.0f, 0.0f);
            glRotatef(lada.pitch, 1.0f, 0.0f, 0.0f);
            glRotatef(lada.roll, 0.0f, 0.0f, 1.0f);

            if (modelLoaded) {
                // ЕСЛИ МОДЕЛЬ ЗАГРУЖЕНА
                // Тут можно подгонять масштаб и поворот, если машина стоит криво!

                // 1. Масштаб (подбирай число, если машина огромная/мелкая)
                glScalef(1.0f, 1.0f, 1.0f);

                // 2. Поворот (часто модели смотрят боком или задом)
                glRotatef(180.0f, 0.0f, 1.0f, 0.0f); // Развернуть на 180, если едет багажником

                glCallList(carList); // Рисуем из памяти
            } else {
                // ЕСЛИ НЕТ ФАЙЛА car.obj
                drawBoxCar();
            }
        glPopMatrix();

        // Рисуем трассу
        drawGround(lada.z);

        SDL_GL_SwapWindow(window);

        Uint32 frameTime = SDL_GetTicks() - currentTime;
        if (frameTime < 16) SDL_Delay(16 - frameTime);

        // FPS counting
        fpsFrameCount++;
        if (SDL_GetTicks() - fpsLastTime >= 1000) {
            Uint32 now = SDL_GetTicks();
            float fps = fpsFrameCount * 1000.0f / (now - fpsLastTime);
            // timestamp
            time_t t = time(NULL);
            char ts[64];
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
            int swap = SDL_GL_GetSwapInterval();
            SDL_Log("%s | FPS: %.1f | VSync: %d | Model: %s | verts=%d faces=%d | displayList=%u", ts, fps, swap, modelLoaded?"yes":"no", (int)verts.size(), (int)faces.size(), carList);
            fpsFrameCount = 0;
            fpsLastTime = now;
        }
    }

    if (modelLoaded) glDeleteLists(carList, 1);
    SDL_GL_DeleteContext(gContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
