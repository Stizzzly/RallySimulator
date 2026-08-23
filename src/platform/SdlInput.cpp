#include "SdlInput.h"
#include <algorithm>

namespace rally {

SdlInputAdapter::~SdlInputAdapter() {
    for (SDL_GameController* pad : pads_) SDL_GameControllerClose(pad);
}

void SdlInputAdapter::openControllers() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* pad = SDL_GameControllerOpen(i);
            if (pad) pads_.push_back(pad);
        }
    }
}

float SdlInputAdapter::applyDeadzone(float value) {
    if (value > 0.2f || value < -0.2f) return value;
    return 0.0f;
}

void SdlInputAdapter::push(MenuAction action) { actions_.push_back(action); }

bool SdlInputAdapter::popAction(MenuAction& out) {
    if (actions_.empty()) return false;
    out = actions_.front();
    actions_.pop_front();
    return true;
}

void SdlInputAdapter::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        switch (e.key.keysym.sym) {
        case SDLK_RETURN:
        case SDLK_SPACE: push(MenuAction::Confirm); break;
        case SDLK_ESCAPE: push(MenuAction::Cancel); break;
        case SDLK_UP: push(MenuAction::Up); break;
        case SDLK_DOWN: push(MenuAction::Down); break;
        case SDLK_c: push(MenuAction::ToggleCamera); break;
        case SDLK_r: push(MenuAction::Restart); break;
        default: break;
        }
        return;
    }

    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A:
        case SDL_CONTROLLER_BUTTON_START: push(MenuAction::Confirm); break;
        case SDL_CONTROLLER_BUTTON_B: push(MenuAction::Cancel); break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: push(MenuAction::Up); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: push(MenuAction::Down); break;
        case SDL_CONTROLLER_BUTTON_Y: push(MenuAction::ToggleCamera); break;
        case SDL_CONTROLLER_BUTTON_BACK: push(MenuAction::Restart); break;
        default: break;
        }
    }
}

void SdlInputAdapter::poll(FrameInput& drive) {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    float steer = 0.0f, throttle = 0.0f, brake = 0.0f;

    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) steer -= 1.0f;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) steer += 1.0f;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) throttle = 1.0f;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) brake = 1.0f;

    for (SDL_GameController* pad : pads_) {
        const float stickX = applyDeadzone(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0f);
        const float stickY = applyDeadzone(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0f);

        steer += stickX;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) steer -= 1.0f;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) steer += 1.0f;

        const float rt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32768.0f;
        const float lt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32768.0f;
        throttle = std::max(throttle, rt > 0.1f ? rt : 0.0f);
        brake = std::max(brake, lt > 0.1f ? lt : 0.0f);
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) throttle = 1.0f;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) brake = 1.0f;

        // Наклон стика вверх/вниз как шаг по меню (по фронту).
        if (stickYPrev_ > -0.5f && stickY <= -0.5f) push(MenuAction::Up);
        if (stickYPrev_ < 0.5f && stickY >= 0.5f) push(MenuAction::Down);
        stickYPrev_ = stickY;
    }

    drive.steer = std::max(-1.0f, std::min(1.0f, steer));
    drive.throttle = std::max(0.0f, std::min(1.0f, throttle));
    drive.brake = std::max(0.0f, std::min(1.0f, brake));
}

} // namespace rally
