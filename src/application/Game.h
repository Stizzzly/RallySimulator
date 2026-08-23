#pragma once

#include "../domain/RaceSession.h"
#include "../domain/TrackModel.h"
#include "ProfileStore.h"
#include <string>
#include <vector>

namespace rally {

// Экранные состояния игры (см. GDD: ProfileSelect, StageSelect, Race, Pause, Results).
enum class GameState { ProfileSelect, StageSelect, Race, Pause, Results };

// Семантические действия меню; адаптер ввода переводит в них клавиши и кнопки.
enum class MenuAction { Up, Down, Confirm, Cancel, Restart, ToggleCamera };

class Game {
public:
    Game(const IProfileStore& store, std::string profileId);

    // Каталог этапов из stages/*.stage; порядок задаёт индексы профиля.
    void setStages(std::vector<StageConfig> stages);

    GameState state() const { return state_; }
    const Profile& profile() const { return profile_; }
    int selectedStage() const { return selectedStage_; }
    const std::vector<StageConfig>& stages() const { return stages_; }
    const StageConfig& stage(int index) const;
    const StageConfig& activeStage() const { return stage(selectedStage_); }
    bool hoodCamera() const { return hoodCamera_; }

    // Меню и паузы.
    void handleAction(MenuAction action);
    // Переход к выбранному этапу (пересоздаёт сессию и модель трассы).
    void startRace();

    // Кадр гонки. В меню ничего не делает.
    void update(float deltaSeconds, const DriveInput& input);

    // Данные для HUD и экрана результатов.
    const RaceSession& session() const { return session_; }
    const TrackModel& track() const { return track_; }
    float checkpointFlash() const { return checkpointFlash_; } // секунды до исчезновения
    bool finishedThisRun() const { return resultShown_; }
    const RaceResult& result() const { return result_; }
    float bestTimeOfSelectedStage() const;

private:
    void finishRace();

    const IProfileStore& store_;
    std::string profileId_;
    Profile profile_;
    std::vector<StageConfig> stages_;
    int selectedStage_ = 0;
    GameState state_ = GameState::ProfileSelect;
    bool hoodCamera_ = false;

    RaceSession session_;
    TrackModel track_{ "" };
    RaceResult result_;
    bool resultShown_ = false;
    float checkpointFlash_ = 0.0f;
};

} // namespace rally
