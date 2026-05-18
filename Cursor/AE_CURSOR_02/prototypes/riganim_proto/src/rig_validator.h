#pragma once

#include "rig_types.h"

#include <vector>

namespace aerigp1 {

struct RigValidator {
    static std::vector<std::string> Validate(const RigDocument& doc);
    static void PrintToDebug(const std::vector<std::string>& messages);
};

} // namespace aerigp1
