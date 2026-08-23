#include "../src/application/Game.h"
#include <cassert>

namespace {

class FakeProfileStore final : public rally::IProfileStore {
public:
    rally::Profile load(const std::string&) const override { return stored; }
    bool save(const std::string&, const rally::Profile& profile) const override {
        stored = profile;
        ++saves;
        return true;
    }
    mutable int saves = 0;
    mutable rally::Profile stored;
};

} // namespace

int main() {
    using namespace rally;

    FakeProfileStore store;
    Game game(store, "test");

    StageConfig stage;
    stage.name = "Sprint";
    stage.theme = "mountain";
    stage.length = 100;
    stage.checkpointInterval = 25;
    stage.goldTime = 9999.0f; // любое завершение даёт золото
    stage.silverTime = 10000.0f;
    stage.bronzeTime = 10001.0f;

    game.setStages({stage, stage, stage});
    assert(game.state() == GameState::ProfileSelect);

    // Меню: профиль -> выбор этапа.
    game.handleAction(MenuAction::Confirm);
    assert(game.state() == GameState::StageSelect);

    // Заблокированный этап не запускается.
    game.handleAction(MenuAction::Down);
    assert(game.selectedStage() == 1);
    game.handleAction(MenuAction::Confirm);
    assert(game.state() == GameState::StageSelect);

    // Возврат и выбор первого этапа.
    game.handleAction(MenuAction::Cancel);
    assert(game.state() == GameState::ProfileSelect);
    game.handleAction(MenuAction::Confirm);
    game.handleAction(MenuAction::Up); // цикл назад к 0
    assert(game.selectedStage() == 0);
    game.handleAction(MenuAction::Confirm);
    assert(game.state() == GameState::Race);

    // Камера переключается в гонке.
    const bool hoodBefore = game.hoodCamera();
    game.handleAction(MenuAction::ToggleCamera);
    assert(game.hoodCamera() != hoodBefore);

    // Пауза, рестарт из паузы, снова пауза-продолжить.
    game.handleAction(MenuAction::Cancel);
    assert(game.state() == GameState::Pause);
    game.handleAction(MenuAction::Restart);
    assert(game.state() == GameState::Race);
    game.handleAction(MenuAction::Cancel);
    game.handleAction(MenuAction::Confirm);
    assert(game.state() == GameState::Race);

    // Гоним вперёд до финиша.
    DriveInput gas;
    gas.throttle = 1.0f;
    for (int i = 0; i < 2000 && game.state() == GameState::Race; ++i)
        game.update(0.05f, gas);
    assert(game.state() == GameState::Results);
    assert(game.session().isFinished());
    assert(store.saves >= 1);

    // Результат: лучшее время записано, медаль золотая, следующий этап открыт.
    assert(game.result().newBest);
    assert(game.result().medal == Medal::Gold);
    assert(store.stored.bestTimes[0] > 0.0f);
    assert(store.stored.medals[0] == Medal::Gold);
    assert(store.stored.unlockedStages == 2);

    // Из результатов обратно в выбор этапа.
    game.handleAction(MenuAction::Confirm);
    assert(game.state() == GameState::StageSelect);

    return 0;
}
