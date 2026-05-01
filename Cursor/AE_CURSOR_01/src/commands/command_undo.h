#pragma once

#include <memory>
#include <vector>

#include "modeling/modeling_commands.h"

namespace aetdp1::commands {

class CommandHistory {
    std::vector<std::unique_ptr<ICommand>> undo_;
    std::vector<std::unique_ptr<ICommand>> redo_;
    static constexpr std::size_t kMaxUndo = 256;

public:
    void Clear();
    void Execute(std::unique_ptr<ICommand> cmd, CommandContext& ctx);

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

    void Undo(CommandContext& ctx);
    void Redo(CommandContext& ctx);

    // Factories (declared in command_undo.cpp)
    static std::unique_ptr<ICommand> MakeReplaceSelection(selection::SelectionState before, selection::SelectionState after);
    static std::unique_ptr<ICommand> MakeSetActiveObject(ObjectId before, ObjectId after);

    static std::unique_ptr<ICommand> MakeCreateObject(scene::ObjectData created);
    static std::unique_ptr<ICommand> MakeDeleteObject(ObjectId id, scene::ObjectData snapshot, selection::SelectionState sel_snapshot);
    static std::unique_ptr<ICommand> MakeDuplicateObject(ObjectId source_id);

    static std::unique_ptr<ICommand> MakeSetTransform(ObjectId id, Transform before, Transform after);
    static std::unique_ptr<ICommand> MakeSetFaceColors(ObjectId id, scene::FaceColors before, scene::FaceColors after);

    static std::unique_ptr<ICommand> MakeNudgeVertex(ObjectId id, std::uint8_t vertex_index, DirectX::XMFLOAT3 before, DirectX::XMFLOAT3 after);

    static std::unique_ptr<ICommand> MakeUpsertTransformKeyframe(ObjectId id, animation::TransformKeyframe key, bool replaced,
                                                                   animation::TransformKeyframe previous_if_replaced);
    static std::unique_ptr<ICommand> MakeDeleteKeyframe(ObjectId id, animation::TransformKeyframe removed, int track_index_hint);

    static std::unique_ptr<ICommand> MakeSetCurrentTime(float before, float after);
};

} // namespace aetdp1::commands
