#pragma once
#include "../domain/RaceTypes.h"
#include <array>
#include <string>

namespace rally {
struct Profile { int unlockedStages = 1; std::array<float, 3> bestTimes{{0,0,0}}; std::array<Medal, 3> medals{{Medal::None, Medal::None, Medal::None}}; };
class IProfileStore { public: virtual ~IProfileStore() = default; virtual Profile load(const std::string& id) const = 0; virtual bool save(const std::string& id, const Profile& profile) const = 0; };
class FileProfileStore final : public IProfileStore { public: explicit FileProfileStore(std::string directory) : directory_(std::move(directory)) {} Profile load(const std::string& id) const override; bool save(const std::string& id, const Profile& profile) const override; private: std::string directory_; };
} // namespace rally
