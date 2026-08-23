#include "Game.h"
#include "Progression.h"
#include <algorithm>
#include <cmath>

namespace rally {

Game::Game(const IProfileStore& store, std::string profileId)
    : store_(store), profileId_(std::move(profileId)) {
    profile_ = store_.load(profileId_);
}

void Game::setStages(std::vector<StageConfig> stages) {
    stages_ = std::move(stages);
    if (selectedStage_ >= static_cast<int>(stages_.size())) selectedStage_ = 0;
}

const StageConfig& Game::stage(int index) const {
    // Каталог фиксирован (3 этапа в v1), но защищаемся от пустого вектора.
    if (stages_.empty()) {
        static const StageConfig fallback;
        return fallback;
    }
    return stages_[static_cast<size_t>(std::max(0, std::min(index, static_cast<int>(stages_.size()) - 1)))];
}

float Game::bestTimeOfSelectedStage() const {
    if (selectedStage_ < 0 || selectedStage_ >= static_cast<int>(profile_.bestTimes.size())) return 0.0f;
    return profile_.bestTimes[static_cast<size_t>(selectedStage_)];
}

void Game::handleAction(MenuAction action) {
    switch (state_) {
    case GameState::ProfileSelect:
        if (action == MenuAction::Confirm) state_ = GameState::StageSelect;
        break;

    case GameState::StageSelect: {
        const int count = static_cast<int>(stages_.size());
        if (count <= 0) break;
        if (action == MenuAction::Up) selectedStage_ = (selectedStage_ + count - 1) % count;
        if (action == MenuAction::Down) selectedStage_ = (selectedStage_ + 1) % count;
        if (action == MenuAction::Confirm
            && selectedStage_ < profile_.unlockedStages)
            startRace();
        if (action == MenuAction::Cancel) state_ = GameState::ProfileSelect;
        break;
    }

    case GameState::Race:
        if (action == MenuAction::Cancel)
            state_ = GameState::Pause;
        else if (action == MenuAction::ToggleCamera) hoodCamera_ = !hoodCamera_;
        break;

    case GameState::Pause:
        if (action == MenuAction::Confirm || action == MenuAction::Cancel)
            state_ = GameState::Race;
        if (action == MenuAction::Restart) startRace();
        break;

    case GameState::Results:
        if (action == MenuAction::Confirm || action == MenuAction::Cancel)
            state_ = GameState::StageSelect;
        break;
    }
}

void Game::startRace() {
    track_ = TrackModel(activeStage().theme);
    session_.start(activeStage(), track_.startingCar());
    result_ = RaceResult{};
    resultShown_ = false;
    checkpointFlash_ = 0.0f;
    state_ = GameState::Race;
}

void Game::update(float dt, const DriveInput& input) {
    if (checkpointFlash_ > 0.0f) checkpointFlash_ = std::max(0.0f, checkpointFlash_ - dt);
    if (state_ != GameState::Race) return;

    const CarState& car = session_.car();
    const SurfaceType surface = track_.surfaceAt(car.x, car.z);
    const float groundY = track_.heightAt(car.x, car.z);
    session_.update(dt, input, surface, groundY);

    const bool crossed = session_.updateProgress(std::max(0.0f, car.z));
    if (crossed) checkpointFlash_ = 1.6f;

    if (session_.isFinished() && !resultShown_) finishRace();
}

void Game::finishRace() {
    resultShown_ = true;
    result_ = session_.finish(bestTimeOfSelectedStage());
    applyRaceResult(profile_, selectedStage_, result_);
    store_.save(profileId_, profile_);
    state_ = GameState::Results;
}

} // namespace rally
