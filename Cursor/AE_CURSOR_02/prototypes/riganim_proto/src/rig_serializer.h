#pragma once

#include "rig_types.h"

#include <string>

namespace aerigp1 {

struct LoadResult {
    bool ok{false};
    std::string error;
};

struct RigSerializer {
    static bool SaveRig(const RigDocument& doc, const std::string& path);
    static LoadResult LoadRig(RigDocument& doc, const std::string& path);
    static bool SaveAnim(const RigDocument& doc, int clip_index, const std::string& path);
    static LoadResult LoadAnim(RigDocument& doc, const std::string& path);
};

} // namespace aerigp1
