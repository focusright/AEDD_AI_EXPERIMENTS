#include "command_undo.h"

#include <algorithm>
#include <utility>

namespace aetdp1::commands {
namespace {

static void RestoreSelection(selection::SelectionState& dst, const selection::SelectionState& src) {
    dst.selected = src.selected;
    dst.active_object = src.active_object;
}

static void CleanupSelectionAfterDelete(selection::SelectionState& sel, scene::SceneData& scene, ObjectId deleted) {
    sel.selected.erase(std::remove(sel.selected.begin(), sel.selected.end(), deleted), sel.selected.end());
    if (sel.active_object == deleted) {
        sel.active_object = sel.selected.empty() ? kInvalidObjectId : sel.selected.front();
    }
    // Remove any other stale ids (prototype safety).
    for (auto it = sel.selected.begin(); it != sel.selected.end();) {
        if (!scene.TryGet(*it)) {
            if (sel.active_object == *it) {
                sel.active_object = kInvalidObjectId;
            }
            it = sel.selected.erase(it);
        } else {
            ++it;
        }
    }
    if (sel.active_object == kInvalidObjectId && !sel.selected.empty()) {
        sel.active_object = sel.selected.front();
    }
}

class CmdSelectObject final : public ICommand {
    selection::SelectionState before_{};
    selection::SelectionState after_{};

public:
    CmdSelectObject(selection::SelectionState before, selection::SelectionState after)
        : before_(std::move(before)), after_(std::move(after)) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.selection) {
            return;
        }
        RestoreSelection(*ctx.selection, after_);
    }
    void Revert(CommandContext& ctx) override {
        if (!ctx.selection) {
            return;
        }
        RestoreSelection(*ctx.selection, before_);
    }
};

class CmdSetActiveObject final : public ICommand {
    ObjectId before_{kInvalidObjectId};
    ObjectId after_{kInvalidObjectId};

public:
    CmdSetActiveObject(ObjectId before, ObjectId after) : before_(before), after_(after) {}

    void Apply(CommandContext& ctx) override {
        if (ctx.selection) {
            ctx.selection->active_object = after_;
        }
    }
    void Revert(CommandContext& ctx) override {
        if (ctx.selection) {
            ctx.selection->active_object = before_;
        }
    }
};

class CmdCreateObject final : public ICommand {
    scene::ObjectData created_{};

public:
    explicit CmdCreateObject(scene::ObjectData created) : created_(std::move(created)) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        ctx.scene->objects.push_back(created_);
    }
    void Revert(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        ctx.scene->RemoveById(created_.id);
        if (ctx.selection) {
            CleanupSelectionAfterDelete(*ctx.selection, *ctx.scene, created_.id);
        }
    }
};

class CmdDeleteObject final : public ICommand {
    ObjectId id_{kInvalidObjectId};
    scene::ObjectData snapshot_{};
    selection::SelectionState sel_before_{};

public:
    CmdDeleteObject(ObjectId id, scene::ObjectData snapshot, selection::SelectionState sel_before)
        : id_(id), snapshot_(std::move(snapshot)), sel_before_(std::move(sel_before)) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.scene || !ctx.selection) {
            return;
        }
        ctx.scene->RemoveById(id_);
        CleanupSelectionAfterDelete(*ctx.selection, *ctx.scene, id_);
    }

    void Revert(CommandContext& ctx) override {
        if (!ctx.scene || !ctx.selection) {
            return;
        }
        ctx.scene->objects.push_back(snapshot_);
        RestoreSelection(*ctx.selection, sel_before_);
    }
};

class CmdDuplicateObject final : public ICommand {
    ObjectId source_{kInvalidObjectId};
    scene::ObjectData created_snapshot_{};
    bool snapshot_valid_{false};

public:
    explicit CmdDuplicateObject(ObjectId source) : source_(source) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        if (!snapshot_valid_) {
            const scene::ObjectData* src = ctx.scene->TryGet(source_);
            if (!src) {
                return;
            }
            created_snapshot_ = *src;
            created_snapshot_.id = ctx.scene->AllocateId();
            created_snapshot_.name = created_snapshot_.name + " Copy";
            created_snapshot_.transform.translation.x += 0.35f;
            snapshot_valid_ = true;
            ctx.scene->objects.push_back(created_snapshot_);
            return;
        }

        // Redo path: reinsert the same snapshot (stable id).
        ctx.scene->objects.push_back(created_snapshot_);
    }

    void Revert(CommandContext& ctx) override {
        if (!snapshot_valid_ || !ctx.scene) {
            return;
        }
        ctx.scene->RemoveById(created_snapshot_.id);
        if (ctx.selection) {
            CleanupSelectionAfterDelete(*ctx.selection, *ctx.scene, created_snapshot_.id);
        }
    }
};

class CmdSetTransform final : public ICommand {
    ObjectId id_{kInvalidObjectId};
    Transform before_{};
    Transform after_{};

public:
    CmdSetTransform(ObjectId id, Transform before, Transform after) : id_(id), before_(before), after_(after) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        if (scene::ObjectData* o = ctx.scene->TryGet(id_)) {
            o->transform = after_;
        }
    }
    void Revert(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        if (scene::ObjectData* o = ctx.scene->TryGet(id_)) {
            o->transform = before_;
        }
    }
};

class CmdSetFaceColors final : public ICommand {
    ObjectId id_{kInvalidObjectId};
    scene::FaceColors before_{};
    scene::FaceColors after_{};

public:
    CmdSetFaceColors(ObjectId id, scene::FaceColors before, scene::FaceColors after)
        : id_(id), before_(std::move(before)), after_(std::move(after)) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        if (scene::ObjectData* o = ctx.scene->TryGet(id_)) {
            o->face_colors = after_;
        }
    }

    void Revert(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        if (scene::ObjectData* o = ctx.scene->TryGet(id_)) {
            o->face_colors = before_;
        }
    }
};

class CmdNudgeVertex final : public ICommand {
    ObjectId id_{kInvalidObjectId};
    std::uint8_t vertex_index_{0};
    DirectX::XMFLOAT3 before_{};
    DirectX::XMFLOAT3 after_{};

public:
    CmdNudgeVertex(ObjectId id, std::uint8_t vertex_index, DirectX::XMFLOAT3 before, DirectX::XMFLOAT3 after)
        : id_(id), vertex_index_(vertex_index), before_(before), after_(after) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        if (scene::ObjectData* o = ctx.scene->TryGet(id_)) {
            if (vertex_index_ < 8) {
                o->local_box.v[vertex_index_] = after_;
            }
        }
    }
    void Revert(CommandContext& ctx) override {
        if (!ctx.scene) {
            return;
        }
        if (scene::ObjectData* o = ctx.scene->TryGet(id_)) {
            if (vertex_index_ < 8) {
                o->local_box.v[vertex_index_] = before_;
            }
        }
    }
};

class CmdAddKeyframe final : public ICommand {
    ObjectId id_{kInvalidObjectId};
    animation::TransformKeyframe key_{};
    bool replaced_{false};
    animation::TransformKeyframe previous_{};

public:
    CmdAddKeyframe(ObjectId id, animation::TransformKeyframe key, bool replaced, animation::TransformKeyframe previous)
        : id_(id), key_(key), replaced_(replaced), previous_(previous) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.anim) {
            return;
        }
        ctx.anim->UpsertKeyframe(id_, key_);
    }

    void Revert(CommandContext& ctx) override {
        if (!ctx.anim) {
            return;
        }
        animation::TransformTrack* tr = ctx.anim->TryGetTrack(id_);
        if (!tr) {
            return;
        }
        if (replaced_) {
            // Restore previous key payload at same time.
            ctx.anim->UpsertKeyframe(id_, previous_);
            return;
        }
        ctx.anim->TryDeleteKeyframeAtTime(id_, key_.time_seconds);
    }
};

class CmdDeleteKeyframe final : public ICommand {
    ObjectId id_{kInvalidObjectId};
    animation::TransformKeyframe removed_{};

public:
    CmdDeleteKeyframe(ObjectId id, animation::TransformKeyframe removed) : id_(id), removed_(removed) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.anim) {
            return;
        }
        ctx.anim->TryDeleteKeyframeAtTime(id_, removed_.time_seconds);
    }

    void Revert(CommandContext& ctx) override {
        if (!ctx.anim) {
            return;
        }
        ctx.anim->UpsertKeyframe(id_, removed_);
    }
};

class CmdSetCurrentTime final : public ICommand {
    float before_{0.f};
    float after_{0.f};

public:
    CmdSetCurrentTime(float before, float after) : before_(before), after_(after) {}

    void Apply(CommandContext& ctx) override {
        if (!ctx.anim) {
            return;
        }
        ctx.anim->current_time_seconds = after_;
    }
    void Revert(CommandContext& ctx) override {
        if (!ctx.anim) {
            return;
        }
        ctx.anim->current_time_seconds = before_;
    }
};

} // namespace

void CommandHistory::Clear() {
    undo_.clear();
    redo_.clear();
}

void CommandHistory::Execute(std::unique_ptr<ICommand> cmd, CommandContext& ctx) {
    if (!cmd) {
        return;
    }
    cmd->Apply(ctx);
    undo_.push_back(std::move(cmd));
    if (undo_.size() > kMaxUndo) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
}

void CommandHistory::Undo(CommandContext& ctx) {
    if (undo_.empty()) {
        return;
    }
    std::unique_ptr<ICommand> cmd = std::move(undo_.back());
    undo_.pop_back();
    cmd->Revert(ctx);
    redo_.push_back(std::move(cmd));
}

void CommandHistory::Redo(CommandContext& ctx) {
    if (redo_.empty()) {
        return;
    }
    std::unique_ptr<ICommand> cmd = std::move(redo_.back());
    redo_.pop_back();
    cmd->Apply(ctx);
    undo_.push_back(std::move(cmd));
}

std::unique_ptr<ICommand> CommandHistory::MakeReplaceSelection(selection::SelectionState before, selection::SelectionState after) {
    return std::make_unique<CmdSelectObject>(std::move(before), std::move(after));
}

std::unique_ptr<ICommand> CommandHistory::MakeSetActiveObject(ObjectId before, ObjectId after) {
    return std::make_unique<CmdSetActiveObject>(before, after);
}

std::unique_ptr<ICommand> CommandHistory::MakeCreateObject(scene::ObjectData created) {
    return std::make_unique<CmdCreateObject>(std::move(created));
}

std::unique_ptr<ICommand> CommandHistory::MakeDeleteObject(ObjectId id, scene::ObjectData snapshot, selection::SelectionState sel_snapshot) {
    return std::make_unique<CmdDeleteObject>(id, std::move(snapshot), std::move(sel_snapshot));
}

std::unique_ptr<ICommand> CommandHistory::MakeDuplicateObject(ObjectId source_id) {
    return std::make_unique<CmdDuplicateObject>(source_id);
}

std::unique_ptr<ICommand> CommandHistory::MakeSetTransform(ObjectId id, Transform before, Transform after) {
    return std::make_unique<CmdSetTransform>(id, before, after);
}

std::unique_ptr<ICommand> CommandHistory::MakeSetFaceColors(ObjectId id, scene::FaceColors before, scene::FaceColors after) {
    return std::make_unique<CmdSetFaceColors>(id, std::move(before), std::move(after));
}

std::unique_ptr<ICommand> CommandHistory::MakeNudgeVertex(ObjectId id, std::uint8_t vertex_index, DirectX::XMFLOAT3 before, DirectX::XMFLOAT3 after) {
    return std::make_unique<CmdNudgeVertex>(id, vertex_index, before, after);
}

std::unique_ptr<ICommand> CommandHistory::MakeUpsertTransformKeyframe(ObjectId id, animation::TransformKeyframe key, bool replaced,
                                                                      animation::TransformKeyframe previous_if_replaced) {
    return std::make_unique<CmdAddKeyframe>(id, key, replaced, previous_if_replaced);
}

std::unique_ptr<ICommand> CommandHistory::MakeDeleteKeyframe(ObjectId id, animation::TransformKeyframe removed, int track_index_hint) {
    (void)track_index_hint;
    return std::make_unique<CmdDeleteKeyframe>(id, removed);
}

std::unique_ptr<ICommand> CommandHistory::MakeSetCurrentTime(float before, float after) {
    return std::make_unique<CmdSetCurrentTime>(before, after);
}

} // namespace aetdp1::commands
