#pragma once

namespace aetdp1::app {
struct AppState;
}

namespace aetdp1::imgui_layer {

void BuildFrame(app::AppState& app, float delta_seconds);

}
