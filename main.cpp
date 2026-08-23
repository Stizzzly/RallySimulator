// Тонкий запускатель: создаёт окно и адаптеры, крутит цикл.
// Игровой логики здесь нет — состояния в rally::Game, ввод в SdlInputAdapter,
// рендер в Renderer (см. AGENTS.md).

#include <SDL2/SDL.h>
#include "application/Game.h"
#include "platform/Renderer.h"
#include "platform/SdlInput.h"
#include "platform/StageFiles.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow("RallySimulator - Volterra Racing",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    rally::Renderer renderer;
    if (!renderer.init(window)) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    renderer.loadAssets();

    // Каталог этапов из stages/*.stage; фолбэк, чтобы игра стартовала всегда.
    std::vector<rally::StageConfig> stages = rally::loadStagesFromDirectory("stages");
    if (stages.empty()) stages.push_back(rally::StageConfig());

    rally::FileProfileStore store("profiles");
    rally::Game game(store, "default");
    game.setStages(std::move(stages));

    rally::SdlInputAdapter input;
    input.openControllers();

    bool quit = false;
    Uint32 lastTime = SDL_GetTicks();
    SDL_Event e;
    rally::FrameInput frame;
    rally::MenuAction action;

    while (!quit) {
        const Uint32 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;
        if (dt > 0.05f) dt = 0.05f;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            else input.handleEvent(e);
        }
        while (input.popAction(action)) game.handleAction(action);

        input.poll(frame);
        const rally::DriveInput drive{frame.steer, frame.throttle, frame.brake};
        game.update(dt, drive);
        renderer.renderFrame(dt, game, frame.steer, frame.throttle);

        SDL_GL_SwapWindow(window);
        const Uint32 frameTime = SDL_GetTicks() - now;
        if (frameTime < 16) SDL_Delay(16 - frameTime); // кап в ~60 FPS
    }

    renderer.release();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
