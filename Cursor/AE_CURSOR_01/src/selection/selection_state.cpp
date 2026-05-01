#include "selection_state.h"

#include <algorithm>

namespace aetdp1::selection {

void SelectionState::Clear() {
    selected.clear();
    active_object = kInvalidObjectId;
}

bool SelectionState::IsSelected(ObjectId id) const {
    return std::find(selected.begin(), selected.end(), id) != selected.end();
}

void SelectionState::SetSingleSelection(ObjectId id) {
    selected.clear();
    if (id != kInvalidObjectId) {
        selected.push_back(id);
    }
    active_object = id;
}

void SelectionState::SetActive(ObjectId id) {
    active_object = id;
}

void SelectionState::Remove(ObjectId id) {
    selected.erase(std::remove(selected.begin(), selected.end(), id), selected.end());
    if (active_object == id) {
        active_object = selected.empty() ? kInvalidObjectId : selected.front();
    }
}

} // namespace aetdp1::selection
