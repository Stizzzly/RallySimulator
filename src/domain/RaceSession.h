#pragma once

#include "RaceTypes.h"

namespace rally {

class RaceSession {
public:
    void start(const StageConfig& stage, const CarState& startingCar);
    // groundY — высота поверхности под машиной (в полёте сцепление падает).
    void update(float deltaSeconds, const DriveInput& input, SurfaceType surface, float groundY);
    bool updateProgress(float distance);
    bool isFinished() const { return finished_; }
    int checkpoint() const { return checkpoint_; }
    int checkpointCount() const;
    float elapsed() const { return elapsed_; }
    const CarState& car() const { return car_; }
    RaceResult finish(float previousBest) const;
    static Medal medalFor(const StageConfig& stage, float seconds);

private:
    StageConfig stage_;
    CarState car_;
    float elapsed_ = 0.0f;
    int checkpoint_ = 0;
    bool finished_ = false;
};

} // namespace rally
