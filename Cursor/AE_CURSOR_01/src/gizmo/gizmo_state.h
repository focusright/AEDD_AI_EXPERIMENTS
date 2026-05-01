#pragma once

namespace aetdp1::gizmo {

enum class GizmoAxis : int { None = 0, X = 1, Y = 2, Z = 3 };

struct GizmoState {
    GizmoAxis hovered_axis{GizmoAxis::None};
    GizmoAxis active_axis{GizmoAxis::None};
    bool dragging{false};

    float axis_length_world{1.15f};
    float hit_radius_pixels{10.f};
};

} // namespace aetdp1::gizmo
