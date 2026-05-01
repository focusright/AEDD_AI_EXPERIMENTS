#pragma once

#include <vector>

#include "core/types.h"

namespace aetdp1::selection {

// Prototype selection model: a set (usually 0..1) plus an explicit active object used by inspector/gizmo/timeline.
struct SelectionState {
    std::vector<ObjectId> selected;
    ObjectId active_object{kInvalidObjectId};

    void Clear();
    bool IsSelected(ObjectId id) const;

    void SetSingleSelection(ObjectId id);
    void SetActive(ObjectId id);

    // Removes id from selection; may clear active if it no longer exists in selection.
    void Remove(ObjectId id);
};

} // namespace aetdp1::selection
