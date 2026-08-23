#include "ProfileStore.h"
#include <direct.h>
#include <fstream>

namespace rally {
Profile FileProfileStore::load(const std::string& id) const { Profile p; std::ifstream f(directory_ + "/" + id + ".profile"); if (!f) return p; f >> p.unlockedStages; for (int i=0;i<3;++i) { int medal=0; f >> p.bestTimes[i] >> medal; p.medals[i]=static_cast<Medal>(medal); } return p; }
bool FileProfileStore::save(const std::string& id, const Profile& p) const { _mkdir(directory_.c_str()); std::ofstream f(directory_ + "/" + id + ".profile"); if (!f) return false; f << p.unlockedStages << '\n'; for (int i=0;i<3;++i) f << p.bestTimes[i] << ' ' << static_cast<int>(p.medals[i]) << '\n'; return true; }
} // namespace rally
