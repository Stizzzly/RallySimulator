#pragma once

#include "../application/Game.h"
#include <deque>
#include <vector>
#include <SDL2/SDL.h>

namespace rally {

// Состояние педалей и руля на текущий кадр.
struct FrameInput {
    float steer = 0.0f;
    float throttle = 0.0f;
    float brake = 0.0f;
};

// Адаптер SDL-ввода: клавиатура + геймпад (SDL GameController).
// Переводит события в семантические MenuAction и опрашивает руль/педали.
class SdlInputAdapter {
public:
    SdlInputAdapter() = default;
    ~SdlInputAdapter();

    void openControllers();

    // Вызывается для каждого события; кладёт действия меню в очередь.
    void handleEvent(const SDL_Event& e);

    // Опрос удерживаемых клавиш/осей; также ловит наклон стика вверх/вниз как Up/Down.
    void poll(FrameInput& drive);

    // Очередь действий меню.
    bool popAction(MenuAction& out);

private:
    void push(MenuAction action);
    static float applyDeadzone(float value);

    std::deque<MenuAction> actions_;
    std::vector<SDL_GameController*> pads_;
    float stickYPrev_ = 0.0f;
};

} // namespace rally
