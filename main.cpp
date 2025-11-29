// Подключаем SDL.
// Так как мы указали include_directories, угловые скобки найдут файл.
#include <SDL.h>
#include <iostream>

// ВАЖНО: Сигнатура main должна быть именно такой!
// int main(int argc, char* argv[])
// Если напишешь void main() или просто int main(), SDL выдаст ошибку линковки.
int main(int argc, char* argv[]) {

    // 1. Инициализация SDL
    // SDL_INIT_VIDEO включает подсистему событий и окон
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Critical Error", SDL_GetError(), NULL);
        return 1;
    }

    // 2. Создание окна
    // Ширина 640, Высота 480, Позиция: по центру
    SDL_Window* window = SDL_CreateWindow(
        "Sega Rally Clone (XP)",    // Заголовок
        SDL_WINDOWPOS_CENTERED,     // X
        SDL_WINDOWPOS_CENTERED,     // Y
        640, 480,                   // W, H
        SDL_WINDOW_SHOWN            // Флаги (просто показать окно)
    );

    if (window == nullptr) {
        std::cerr << "Ошибка создания окна: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // 3. Получаем поверхность окна (Surface) для рисования (пока без OpenGL)
    SDL_Surface* screenSurface = SDL_GetWindowSurface(window);

    // Зальем окно синим цветом (R=0, G=0, B=255)
    SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0, 0, 255));

    // Обновляем окно
    SDL_UpdateWindowSurface(window);

    // 4. Цикл ожидания (чтобы окно не закрылось сразу)
    bool isRunning = true;
    SDL_Event e;

    while (isRunning) {
        // Читаем очередь событий
        while (SDL_PollEvent(&e) != 0) {
            // Если нажали крестик
            if (e.type == SDL_QUIT) {
                isRunning = false;
            }
            // Если нажали ESC
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                isRunning = false;
            }
        }
        // Тут небольшая задержка, чтобы не грузить CPU в простое
        SDL_Delay(10);
    }

    // 5. Очистка
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
