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
#include <cstdlib>

// --- КОНСТАНТЫ ---
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const float DEG2RAD = 3.14159f / 180.0f;
const float ROAD_WIDTH = 10.0f;
const float CURB_WIDTH = 1.5f;

// --- МАГИЯ КАРМАКА (Fast Inverse Square Root) ---
float Q_rsqrt(float number) {
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    i  = * ( long * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
    return y;
}

// --- СТРУКТУРЫ ---
struct Vector3 { float x, y, z; };
struct Face { int v1, v2, v3; };

struct TerrainVertex {
    float x, y, z;
    float r, g, b;
};

struct Car {
    float x = 0.0f, y = 1.0f, z = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    float angle = 0.0f;
    float pitch = 0.0f, roll = 0.0f;

    float speed = 0.0f;
    float acceleration = 35.0f;
    float maxSpeed = 55.0f;
    float turnSpeed = 100.0f;
    float frictionAsphalt = 1.5f;
    float frictionGrass = 5.0f;
};

// Глобальный массив для трассы (для Turion 64!)
std::vector<TerrainVertex> trackData;

// --- ГЕНЕРАЦИЯ ТРАССЫ И ПОКРЫТИЯ ---

// 1. Более извилистая трасса (смесь 3 частот)
float getTrackCenterX(float z) {
    // Плавные изгибы + резкие повороты
    return sinf(z * 0.015f) * 40.0f
         + sinf(z * 0.04f) * 20.0f
         + cosf(z * 0.08f) * 10.0f; // Эта добавляет резкие "шпильки"
}

// 2. Отодвинутые горы
float getTerrainHeight(float x, float z) {
    float centerX = getTrackCenterX(z);
    float dist = fabsf(x - centerX);

    // Плавные холмы вдоль всей трассы
    float baseHeight = sinf(z * 0.03f) * 3.0f;

    if (dist < ROAD_WIDTH + CURB_WIDTH) return baseHeight + 0.2f; // Плоская дорога

    // Равнина / Обочина (шириной 25 метров)
    if (dist < ROAD_WIDTH + CURB_WIDTH + 25.0f) {
        return baseHeight + 0.2f + sinf(x * 0.4f) * 0.3f; // Легкие кочки на траве
    }

    // Горы начинаются только далеко по краям
    float wallDist = dist - (ROAD_WIDTH + CURB_WIDTH + 25.0f);
    return baseHeight + 0.2f + (wallDist * wallDist * 0.05f) + sinf(x)*0.5f;
}

// 3. Определяем тип поверхности под колесами (0 = Асфальт, 1 = Гравий, 2 = Трава)
int getSurfaceType(float x, float z) {
    float center = getTrackCenterX(z);
    float dist = fabsf(x - center);

    // Если съехали с дороги
    if (dist > ROAD_WIDTH + 0.5f) return 2; // ТРАВА

    // Математический шум для генерации пятен гравия
    // Комбинируем синусы, чтобы гравий появлялся хаотичными пятнами вдоль трассы
    float gravelNoise = sinf(z * 0.015f) + cosf(z * 0.045f);

    if (gravelNoise > 0.8f) return 1; // ГРАВИЙ (Неожиданный участок!)

    return 0; // АСФАЛЬТ
}

// Кэшируем трассу при загрузке
void generateLevel(int lengthZ) {
    int stepX = 4; // Шаг 4 метра для ретро-оптимизации
    int stepZ = 4;

    for (int z = -20; z < lengthZ; z += stepZ) {
        float centerX1 = getTrackCenterX((float)z);
        float centerX2 = getTrackCenterX((float)(z + stepZ));

        for (int x = -40; x < 40; x += stepX) {
            float curX1 = centerX1 + x;
            float curX2 = centerX2 + x;

            auto getColor = [](float x_pos, float z_pos, float center) {
                float dist = fabsf(x_pos - center);
                TerrainVertex v = {x_pos, getTerrainHeight(x_pos, z_pos), z_pos, 0, 0, 0};

                int surface = getSurfaceType(x_pos, z_pos);

                if (surface == 0) { // АСФАЛЬТ
                    float grey = 0.25f + sinf(x_pos * 10.0f) * 0.02f;
                    if (dist < 0.4f && (int(z_pos) / 4) % 2 == 0) { v.r=1.0f; v.g=1.0f; v.b=1.0f; } // Разметка
                    else { v.r=grey; v.g=grey; v.b=grey+0.05f; }
                }
                else if (surface == 1) { // ГРАВИЙ
                    // Желто-коричневый цвет с шумом
                    float dirtNoise = (sinf(x_pos * 5.0f) * sinf(z_pos * 5.0f)) * 0.05f;
                    v.r = 0.6f + dirtNoise;
                    v.g = 0.5f + dirtNoise;
                    v.b = 0.4f + dirtNoise;
                }
                else { // ТРАВА И ГОРЫ
                    if (dist < ROAD_WIDTH + CURB_WIDTH) { // Поребрик
                        if ((int(z_pos) / 3) % 2 == 0) { v.r=0.9f; v.g=0.1f; v.b=0.1f; }
                        else { v.r=0.9f; v.g=0.9f; v.b=0.9f; }
                    } else {
                        if ((int(x_pos)/4 + int(z_pos)/4) % 2 == 0) { v.r=0.1f; v.g=0.4f; v.b=0.1f; }
                        else { v.r=0.2f; v.g=0.5f; v.b=0.2f; }
                    }
                }
                return v;
            };

            TerrainVertex v1 = getColor(curX1, z, centerX1);
            TerrainVertex v2 = getColor(curX1 + stepX, z, centerX1);
            TerrainVertex v3 = getColor(curX2, z + stepZ, centerX2);
            TerrainVertex v4 = getColor(curX2 + stepX, z + stepZ, centerX2);

            trackData.push_back(v1); trackData.push_back(v2); trackData.push_back(v3);
            trackData.push_back(v2); trackData.push_back(v4); trackData.push_back(v3);
        }
    }
    SDL_Log("Level RAM usage: %.2f MB", (float)(trackData.size() * sizeof(TerrainVertex)) / 1048576.0f);
}

void drawFastGround(float carZ) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(TerrainVertex), &trackData[0].x);
    glColorPointer(3, GL_FLOAT, sizeof(TerrainVertex), &trackData[0].r);

    // Считаем индексы для отрисовки только нужного куска (culling)
    int stepZ = 4;
    int verticesPerRow = (80 / stepZ) * 6;
    int currentRow = (int)(carZ + 20) / stepZ;

    int startVertex = (currentRow - 5) * verticesPerRow;
    if (startVertex < 0) startVertex = 0;

    int drawCount = 30 * verticesPerRow; // Рисуем 120 метров вперед
    if (startVertex + drawCount > (int)trackData.size()) drawCount = trackData.size() - startVertex;

    if (drawCount > 0) {
        glDrawArrays(GL_TRIANGLES, startVertex, drawCount);
    }

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// --- СИСТЕМА ЧАСТИЦ (ПЫЛЬ И ДЫМ) ---
struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float life, maxLife;
    float size;
    float r, g, b, a;
    bool active = false;
};

const int MAX_PARTICLES = 300;
Particle particles[MAX_PARTICLES];

// Функция спавна одной частицы
void emitDust(float x, float y, float z, float r, float g, float b) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].x = x + ((rand() % 100) / 100.0f - 0.5f); // Немного рандома в позиции
            particles[i].y = y;
            particles[i].z = z + ((rand() % 100) / 100.0f - 0.5f);

            // Разлет в стороны и вверх
            particles[i].vx = ((rand() % 100) / 50.0f - 1.0f) * 3.0f;
            particles[i].vy = ((rand() % 100) / 100.0f) * 4.0f + 1.0f;
            particles[i].vz = ((rand() % 100) / 50.0f - 1.0f) * 3.0f;

            particles[i].maxLife = 0.5f + ((rand() % 100) / 100.0f) * 0.5f; // Живет 0.5-1.0 сек
            particles[i].life = particles[i].maxLife;
            particles[i].size = 0.3f;
            particles[i].r = r; particles[i].g = g; particles[i].b = b;
            break;
        }
    }
}

// Обновление и отрисовка всех частиц
void updateAndDrawParticles(float deltaTime, float camAngle) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Отключаем запись в буфер глубины (чтобы прозрачность не глючила)

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            // Физика частицы
            particles[i].x += particles[i].vx * deltaTime;
            particles[i].y += particles[i].vy * deltaTime;
            particles[i].z += particles[i].vz * deltaTime;
            particles[i].size += 3.0f * deltaTime; // Клубы дыма расширяются
            particles[i].life -= deltaTime;

            // Вычисляем прозрачность (плавно исчезает)
            particles[i].a = (particles[i].life / particles[i].maxLife) * 0.6f;

            if (particles[i].life <= 0.0f) {
                particles[i].active = false;
                continue;
            }

            // Отрисовка Биллборда (квадрат, который всегда смотрит в камеру)
            glPushMatrix();
            glTranslatef(particles[i].x, particles[i].y, particles[i].z);
            glRotatef(-camAngle, 0.0f, 1.0f, 0.0f); // Поворачиваем лицом к камере!

            glColor4f(particles[i].r, particles[i].g, particles[i].b, particles[i].a);
            float s = particles[i].size;
            glBegin(GL_QUADS);
            glVertex3f(-s, -s, 0); glVertex3f(s, -s, 0);
            glVertex3f(s, s, 0); glVertex3f(-s, s, 0);
            glEnd();

            glPopMatrix();
        }
    }
    glDepthMask(GL_TRUE); // Включаем обратно
    glDisable(GL_BLEND);
}

// --- ЗАГРУЗКА OBJ ---
bool loadOBJ(const char* path, std::vector<Vector3>& out_vertices, std::vector<Face>& out_faces) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "v") {
            Vector3 v; ss >> v.x >> v.y >> v.z; out_vertices.push_back(v);
        } else if (prefix == "f") {
            std::string s1, s2, s3; ss >> s1 >> s2 >> s3;
            auto getIdx = [](std::string& s) { return std::stoi(s.substr(0, s.find('/'))) - 1; };
            Face f = {getIdx(s1), getIdx(s2), getIdx(s3)};
            out_faces.push_back(f);
        }
    }
    return true;
}

// Фолбэк-машина
void drawBoxCar() {
    glPushMatrix(); glScalef(0.9f, 0.6f, 2.0f);
    glBegin(GL_QUADS);
    glColor3f(0.8f, 0.0f, 0.0f); glVertex3f(-1,1,-1); glVertex3f(1,1,-1); glVertex3f(1,1,1); glVertex3f(-1,1,1);
    glColor3f(0.2f, 0.2f, 0.2f); glVertex3f(-1,0,-1); glVertex3f(1,0,-1); glVertex3f(1,0,1); glVertex3f(-1,0,1);
    glColor3f(0.6f, 0.0f, 0.0f); glVertex3f(-1,0,1); glVertex3f(1,0,1); glVertex3f(1,1,1); glVertex3f(-1,1,1);
    glVertex3f(-1,0,-1); glVertex3f(1,0,-1); glVertex3f(1,1,-1); glVertex3f(-1,1,-1);
    glVertex3f(-1,0,-1); glVertex3f(-1,0,1); glVertex3f(-1,1,1); glVertex3f(-1,1,-1);
    glVertex3f(1,0,-1); glVertex3f(1,0,1); glVertex3f(1,1,1); glVertex3f(1,1,-1);
    glEnd(); glPopMatrix();
}

void setupPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    GLdouble ymax = zNear * tan(fovy * 3.14159265 / 360.0);
    GLdouble ymin = -ymax;
    GLdouble xmin = ymin * aspect;
    GLdouble xmax = ymax * aspect;
    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void drawWheel() {
    glPushMatrix();
    glScalef(0.15f, 0.3f, 0.3f); // Ширина, Высота, Длина колеса
    glBegin(GL_QUADS);
    glColor3f(0.1f, 0.1f, 0.1f); // Черная резина
    // Рисуем простой кубик (колесо)
    glVertex3f(-1, 1, -1); glVertex3f(1, 1, -1); glVertex3f(1, 1, 1); glVertex3f(-1, 1, 1);
    glVertex3f(-1, -1, -1); glVertex3f(1, -1, -1); glVertex3f(1, -1, 1); glVertex3f(-1, -1, 1);
    glVertex3f(-1, -1, 1); glVertex3f(1, -1, 1); glVertex3f(1, 1, 1); glVertex3f(-1, 1, 1);
    glVertex3f(-1, -1, -1); glVertex3f(1, -1, -1); glVertex3f(1, 1, -1); glVertex3f(-1, 1, -1);
    glVertex3f(-1, -1, -1); glVertex3f(-1, -1, 1); glVertex3f(-1, 1, 1); glVertex3f(-1, 1, -1);
    glVertex3f(1, -1, -1); glVertex3f(1, -1, 1); glVertex3f(1, 1, 1); glVertex3f(1, 1, -1);
    glEnd();
    // Диск колеса (чтобы было видно, как оно крутится/стоит)
    glScalef(1.01f, 0.5f, 0.5f);
    glBegin(GL_QUADS);
    glColor3f(0.8f, 0.8f, 0.8f); // Белый диск
    glVertex3f(-1, -1, 1); glVertex3f(1, -1, 1); glVertex3f(1, 1, 1); glVertex3f(-1, 1, 1);
    glVertex3f(-1, -1, -1); glVertex3f(1, -1, -1); glVertex3f(1, 1, -1); glVertex3f(-1, 1, -1);
    glEnd();
    glPopMatrix();
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_Window* window = SDL_CreateWindow("Sega Rally Clone - Demo 2",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_GLContext gContext = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    GLfloat fogColor[] = {0.4f, 0.6f, 0.9f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 40.0f);
    glFogf(GL_FOG_END, 110.0f); // Отодвинули туман, чтобы видеть трамплин

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setupPerspective(75.0, (double)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1, 200.0);

    // Генерируем трассу на 4 километра!
    SDL_Log("Generating level geometry...");
    generateLevel(4000);

    // Загрузка модели
    std::vector<Vector3> verts;
    std::vector<Face> faces;
    bool modelLoaded = loadOBJ("car.obj", verts, faces);

    GLuint carList = 0;
    if (modelLoaded) {
        carList = glGenLists(1);
        glNewList(carList, GL_COMPILE);
        glBegin(GL_TRIANGLES);
        for (const auto& f : faces) {
            Vector3 v1 = verts[f.v1], v2 = verts[f.v2], v3 = verts[f.v3];

            // Считаем нормаль
            float ux = v2.x - v1.x, uy = v2.y - v1.y, uz = v2.z - v1.z;
            float vx = v3.x - v1.x, vy = v3.y - v1.y, vz = v3.z - v1.z;
            float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;

            // Быстрая нормализация от Джона Кармака
            float invLen = Q_rsqrt(nx*nx + ny*ny + nz*nz);
            nx *= invLen; ny *= invLen; nz *= invLen;

            // --- БАЗОВОЕ ОСВЕЩЕНИЕ ---
            float brightness = 0.5f + (ny > 0 ? ny : 0) * 0.5f; // Свет сверху
            if (nx > 0.5 || nx < -0.5) brightness *= 0.7f;      // Бока темнее

            // --- РАСКРАСКА ЖИГУЛЯ МАТЕМАТИКОЙ ---
            float r = 0.8f, g = 0.1f, b = 0.1f; // Базовый цвет: Раллийный Красный

            // Вычисляем среднюю высоту полигона (y)
            float avgY = (v1.y + v2.y + v3.y) / 3.0f;
            // Вычисляем угол наклона (насколько смотрит вверх/вперед)

            // Если полигон высоко (это крыша или стекла) и имеет наклон
            if (avgY > 0.85f) {
                if (ny < 0.9f) {
                    // Это стекла (лобовое, заднее, боковые окна)
                    r = 0.1f; g = 0.1f; b = 0.1f; // Черная тонировка!
                    brightness = 1.0f; // Стекла не затемняем
                } else {
                    // Это крыша - красим в белый (как у классического ралли)
                    r = 0.9f; g = 0.9f; b = 0.9f;
                }
            }

            // Применяем освещение к цвету
            glColor3f(r * brightness, g * brightness, b * brightness);

            // Рисуем треугольник
            glVertex3f(v1.x, v1.y, v1.z);
            glVertex3f(v2.x, v2.y, v2.z);
            glVertex3f(v3.x, v3.y, v3.z);
        }
        glEnd();
        glEndList();
    }

    Car lada;
    // 1. Ставим машину ровно по центру дороги на старте
    lada.x = getTrackCenterX(0.0f);

    // 2. Вычисляем, куда изгибается трасса впереди (берем точку через 5 метров)
    float dx = getTrackCenterX(5.0f) - lada.x;
    float dz = 5.0f;

    // 3. Разворачиваем машину на 180 градусов (чтобы ехать в +Z)
    // и добавляем угол изгиба дороги
    lada.angle = 180.0f + (atan2(dx, dz) / DEG2RAD);

    // 4. Сразу настраиваем камеру, чтобы она не "догоняла" поворот при старте
    float camX = lada.x, camY = lada.y, camZ = lada.z, camAngle = lada.angle;

    Uint32 lastTime = SDL_GetTicks();
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    bool quit = false;
    SDL_Event e;

    while (!quit) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) quit = true;
        }

        // --- ФИЗИКА И УПРАВЛЕНИЕ ---
        int currentSurface = getSurfaceType(lada.x, lada.z);

        if (abs(lada.speed) > 0.5f) {
            float dir = (lada.speed > 0) ? 1.0f : -1.0f;
            if (keys[SDL_SCANCODE_LEFT])  lada.angle += lada.turnSpeed * deltaTime * dir;
            if (keys[SDL_SCANCODE_RIGHT]) lada.angle -= lada.turnSpeed * deltaTime * dir;
        }

        float rad = lada.angle * DEG2RAD;
        float targetDirX = -sin(rad);
        float targetDirZ = -cos(rad);

        // Настраиваем характеристики от покрытия
        float currentAccel = lada.acceleration;
        float currentFriction = lada.frictionAsphalt;
        float grip = 6.0f * deltaTime; // По умолчанию асфальт

        if (currentSurface == 1) { // ГРАВИЙ
            currentAccel *= 0.8f; // Чуть хуже разгон
            currentFriction = 2.0f;
            grip = 2.0f * deltaTime; // Скользко! Машину несет боком
        }
        else if (currentSurface == 2) { // ТРАВА
            currentAccel *= 0.4f; // Еле едет
            currentFriction = lada.frictionGrass;
            grip = 1.0f * deltaTime; // Очень скользко
        }

        bool isPedalPressed = false;
        if (keys[SDL_SCANCODE_UP])   { lada.speed += currentAccel * deltaTime; isPedalPressed = true; }
        if (keys[SDL_SCANCODE_DOWN]) { lada.speed -= (currentAccel * 0.5f) * deltaTime; isPedalPressed = true; }

        if (!isPedalPressed) {
            lada.speed -= lada.speed * currentFriction * deltaTime;
            if (abs(lada.speed) < 0.1f) lada.speed = 0.0f;
        }

        if (lada.speed > lada.maxSpeed) lada.speed = lada.maxSpeed;
        if (lada.speed < -lada.maxSpeed * 0.5f) lada.speed = -lada.maxSpeed * 0.5f;

        // --- ГРАВИТАЦИЯ И ПРЫЖКИ ---
        float groundY = getTerrainHeight(lada.x, lada.z);
        lada.velY -= 40.0f * deltaTime; // Гравитация
        lada.y += lada.velY * deltaTime;

        if (lada.y <= groundY) {
            lada.y = groundY;
            lada.velY = 0.0f; // Приземлились
        } else {
            grip *= 0.1f; // В полете руль не работает!
        }

        lada.velX += (targetDirX * lada.speed - lada.velX) * grip;
        lada.velZ += (targetDirZ * lada.speed - lada.velZ) * grip;

        lada.x += lada.velX * deltaTime;
        lada.z += lada.velZ * deltaTime;

        // --- ЛОГИКА ПОЯВЛЕНИЯ ПЫЛИ ---
        // Считаем реальную скорость полета машины в пространстве
        float currentSpeed = sqrtf(lada.velX*lada.velX + lada.velZ*lada.velZ);

        // Машина должна быть на земле и ехать быстро, чтобы поднимать пыль
        if (currentSpeed > 10.0f && lada.y <= groundY + 0.1f) {

            // Куда реально летит машина (нормализованный вектор)
            float normVx = lada.velX / currentSpeed;
            float normVz = lada.velZ / currentSpeed;

            // Dot Product: Насколько совпадает то, куда мы смотрим, с тем, куда мы летим
            // 1.0 = едем идеально прямо. Чем меньше, тем сильнее мы едем боком (дрифт)
            float dot = (normVx * targetDirX) + (normVz * targetDirZ);

            bool isOnAsphalt = (currentSurface == 0);
            bool isDrifting = (isOnAsphalt && dot < 0.95f);
            bool isOnGravel = (currentSurface == 1);
            bool isOnGrass  = (currentSurface == 2);

            // Если дрифтим по асфальту, ИЛИ просто едем по гравию/траве
            if (isDrifting || isOnGravel || isOnGrass) {
                float r = isOnAsphalt ? 0.7f : (isOnGravel ? 0.7f : 0.35f);
                float g = isOnAsphalt ? 0.7f : (isOnGravel ? 0.6f : 0.25f);
                float b = isOnAsphalt ? 0.7f : (isOnGravel ? 0.4f : 0.15f);

                // 1. Находим центр задней оси (чуть ближе, чем бампер)
                float rearCenterX = lada.x + targetDirX * 1.5f;
                float rearCenterZ = lada.z + targetDirZ * 1.5f;

                // 2. Вычисляем вектор "Вправо" (перпендикуляр к направлению машины)
                // Если Вперед это (-sin, -cos), то Вправо это (cos, -sin)
                float rightDirX = cos(rad);
                float rightDirZ = -sin(rad);

                // 3. Ширина половины машины (чтобы попасть ровно в колеса)
                float halfWidth = 0.8f;

                // 4. Координаты левого и правого колеса
                float rearLeftX = rearCenterX - rightDirX * halfWidth;
                float rearLeftZ = rearCenterZ - rightDirZ * halfWidth;

                float rearRightX = rearCenterX + rightDirX * halfWidth;
                float rearRightZ = rearCenterZ + rightDirZ * halfWidth;

                // Кидаем пыль из-под КАЖДОГО колеса
                int dustPerWheel = isOnGravel ? 4 : 2;
                for(int i = 0; i < dustPerWheel; i++) {
                    emitDust(rearLeftX, lada.y, rearLeftZ, r, g, b);  // Левое колесо
                    emitDust(rearRightX, lada.y, rearRightZ, r, g, b); // Правое колесо
                }
            }
        }

        // Наклоны кузова
        float noseY = getTerrainHeight(lada.x + targetDirX * 2.0f, lada.z + targetDirZ * 2.0f);
        float sideY = getTerrainHeight(lada.x - targetDirZ * 1.5f, lada.z + targetDirX * 1.5f);
        float targetPitch = (noseY - lada.y) * 30.0f; // Наклон от земли
        float targetRoll = (sideY - lada.y) * 30.0f;
        if(keys[SDL_SCANCODE_LEFT])  targetRoll -= abs(lada.speed) * 0.3f;
        if(keys[SDL_SCANCODE_RIGHT]) targetRoll += abs(lada.speed) * 0.3f;
        lada.pitch += (targetPitch - lada.pitch) * 5.0f * deltaTime;
        lada.roll += (targetRoll - lada.roll) * 5.0f * deltaTime;

        // --- ПЛАВНАЯ КАМЕРА ---
        camX += (lada.x - camX) * 10.0f * deltaTime;
        camY += (lada.y - camY) * 10.0f * deltaTime;
        camZ += (lada.z - camZ) * 10.0f * deltaTime;
        float angleDiff = lada.angle - camAngle;
        while (angleDiff > 180.0f) angleDiff -= 360.0f;
        while (angleDiff < -180.0f) angleDiff += 360.0f;
        camAngle += angleDiff * 5.0f * deltaTime;

        // --- РЕНДЕР ---
        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Устанавливаем камеру
        glTranslatef(0.0f, -2.5f, -7.0f);
        glRotatef(-camAngle, 0.0f, 1.0f, 0.0f);
        glTranslatef(-camX, -camY, -camZ);

        // Рисуем машину
        glPushMatrix();
        glTranslatef(lada.x, lada.y + 0.4f, lada.z);
        glRotatef(lada.angle, 0.0f, 1.0f, 0.0f);
        glRotatef(lada.pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(lada.roll, 0.0f, 0.0f, 1.0f);

        if (modelLoaded) glCallList(carList); else drawBoxCar();

        // --- НОВОЕ: ПРИКРУЧИВАЕМ КОЛЁСА ---
        // Вычисляем угол поворота передних колес для визуала
        float steerAngle = 0.0f;
        if (keys[SDL_SCANCODE_LEFT]) steerAngle = 30.0f;
        if (keys[SDL_SCANCODE_RIGHT]) steerAngle = -30.0f;

        // Переднее левое
        glPushMatrix();
        glTranslatef(-0.8f, -0.1f, -1.5f); // Позиция относительно кузова
        glRotatef(steerAngle, 0.0f, 1.0f, 0.0f); // Поворот руля!
        drawWheel();
        glPopMatrix();

        // Переднее правое
        glPushMatrix();
        glTranslatef(0.8f, -0.1f, -1.5f);
        glRotatef(steerAngle, 0.0f, 1.0f, 0.0f);
        drawWheel();
        glPopMatrix();

        // Заднее левое
        glPushMatrix();
        glTranslatef(-0.8f, -0.1f, 1.5f);
        drawWheel();
        glPopMatrix();

        // Заднее правое
        glPushMatrix();
        glTranslatef(0.8f, -0.1f, 1.5f);
        drawWheel();
        glPopMatrix();

        glPopMatrix();

        // Быстрый рендер уровня
        drawFastGround(lada.z);

        // Рисуем частицы поверх уровня
        updateAndDrawParticles(deltaTime, camAngle);

        SDL_GL_SwapWindow(window);
        Uint32 frameTime = SDL_GetTicks() - currentTime;
        if (frameTime < 16) SDL_Delay(16 - frameTime); // Кап в 60 FPS
    }

    if (modelLoaded) glDeleteLists(carList, 1);
    SDL_GL_DeleteContext(gContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
