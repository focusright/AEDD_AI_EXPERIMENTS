#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <sstream>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <wrl/client.h>

#include "app/app_state.h"
#include "animation/animation_eval.h"
#include "imgui/imgui_layer.h"
#include "modeling/modeling_commands.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

aetdp1::app::AppState* g_app = nullptr;
using Microsoft::WRL::ComPtr;

static std::wstring HResultHex(HRESULT hr) {
    wchar_t buf[32]{};
    swprintf_s(buf, L"0x%08X", static_cast<unsigned>(hr));
    return buf;
}

static std::wstring BuildD3D12Diagnostics() {
    std::wostringstream oss;

    ComPtr<IDXGIFactory4> factory;
    const HRESULT hr_factory = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    oss << L"CreateDXGIFactory1: " << HResultHex(hr_factory) << L"\n";
    if (FAILED(hr_factory)) {
        return oss.str();
    }

    bool any_hw_adapter = false;
    bool hw_device_ok = false;
    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT hr_enum = factory->EnumAdapters1(i, &adapter);
        if (hr_enum == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr_enum)) {
            oss << L"EnumAdapters1[" << i << L"]: " << HResultHex(hr_enum) << L"\n";
            continue;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) {
            continue;
        }
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }
        any_hw_adapter = true;

        ComPtr<ID3D12Device> dev;
        const HRESULT hr_dev = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
        oss << L"Hardware adapter[" << i << L"] '" << desc.Description << L"' D3D12CreateDevice: " << HResultHex(hr_dev) << L"\n";
        if (SUCCEEDED(hr_dev)) {
            hw_device_ok = true;
            break;
        }
    }
    if (!any_hw_adapter) {
        oss << L"No non-software hardware adapters reported by DXGI.\n";
    }

    ComPtr<IDXGIAdapter> warp;
    const HRESULT hr_warp_enum = factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
    oss << L"EnumWarpAdapter: " << HResultHex(hr_warp_enum) << L"\n";
    if (SUCCEEDED(hr_warp_enum)) {
        ComPtr<ID3D12Device> warp_dev;
        const HRESULT hr_warp_dev = D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&warp_dev));
        oss << L"WARP D3D12CreateDevice: " << HResultHex(hr_warp_dev) << L"\n";
    }

    oss << L"Hardware device usable: " << (hw_device_ok ? L"yes" : L"no") << L"\n";
    return oss.str();
}

static void ImGuiSrvAllocFn(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) {
    auto* r = reinterpret_cast<aetdp1::renderer::D3d12Renderer*>(info->UserData);
    r->AllocSrvForImGui(out_cpu, out_gpu);
}

static void ImGuiSrvFreeFn(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    auto* r = reinterpret_cast<aetdp1::renderer::D3d12Renderer*>(info->UserData);
    r->FreeSrvForImGui(cpu, gpu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (g_app && g_app->renderer.device() && wparam != SIZE_MINIMIZED) {
            RECT cr{};
            GetClientRect(hwnd, &cr);
            const UINT ww = static_cast<UINT>(std::max(1L, cr.right - cr.left));
            const UINT hh = static_cast<UINT>(std::max(1L, cr.bottom - cr.top));
            g_app->renderer.BeginResize();
            g_app->renderer.Resize(ww, hh);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void InitDefaultScene(aetdp1::app::AppState& app) {
    aetdp1::scene::ObjectData obj = aetdp1::commands::MakeDefaultNamedObject(app.scene, "Reference Cube");
    app.scene.objects.push_back(std::move(obj));
    app.selection.SetSingleSelection(app.scene.objects.front().id);
    aetdp1::animation::EvaluateTransformTracksAtTime(app.anim, app.scene, app.anim.current_time_seconds);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR, int) {
    ImGui_ImplWin32_EnableDpiAwareness();

    const wchar_t kClassName[] = L"AETDP1_WNDCLASS";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    aetdp1::app::AppState app{};
    g_app = &app;

    HWND hwnd = CreateWindowExW(0, kClassName, L"AETDP1 — Another Engine top-down reference prototype", WS_OVERLAPPEDWINDOW, 80, 80, 1600, 900, nullptr,
                                nullptr, hinst, nullptr);
    if (!hwnd) {
        MessageBoxW(nullptr, L"CreateWindowEx failed", L"AETDP1", MB_ICONERROR);
        return 1;
    }

    app.hwnd = hwnd;
    app.cmd_ctx.scene = &app.scene;
    app.cmd_ctx.selection = &app.selection;
    app.cmd_ctx.anim = &app.anim;

    if (!app.renderer.Create(hwnd)) {
        std::wstring msg = L"D3D12 renderer init failed.\n\nDiagnostics:\n";
        msg += BuildD3D12Diagnostics();
        MessageBoxW(hwnd, msg.c_str(), L"AETDP1", MB_ICONERROR);
        DestroyWindow(hwnd);
        UnregisterClassW(kClassName, hinst);
        return 2;
    }

    InitDefaultScene(app);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo dx12_info{};
    dx12_info.Device = app.renderer.device();
    dx12_info.CommandQueue = app.renderer.command_queue();
    dx12_info.NumFramesInFlight = 2;
    dx12_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    dx12_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    dx12_info.UserData = &app.renderer;
    dx12_info.SrvDescriptorHeap = app.renderer.srv_heap();
    dx12_info.SrvDescriptorAllocFn = &ImGuiSrvAllocFn;
    dx12_info.SrvDescriptorFreeFn = &ImGuiSrvFreeFn;
    if (!ImGui_ImplDX12_Init(&dx12_info)) {
        MessageBoxW(hwnd, L"ImGui_ImplDX12_Init failed", L"AETDP1", MB_ICONERROR);
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        app.renderer.Destroy();
        DestroyWindow(hwnd);
        UnregisterClassW(kClassName, hinst);
        return 3;
    }

    bool running = true;
    while (running) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (app.anim.playback.playing && !app.anim.playback.paused) {
            app.anim.current_time_seconds += io.DeltaTime;
            app.anim.current_time_seconds = std::max(0.f, std::min(app.anim.current_time_seconds, app.anim.duration_seconds));
            aetdp1::animation::EvaluateTransformTracksAtTime(app.anim, app.scene, app.anim.current_time_seconds);
        }

        aetdp1::imgui_layer::BuildFrame(app, io.DeltaTime);

        ImGui::Render();

        app.renderer.ResizeViewportTexture(static_cast<std::uint32_t>(std::max(1, app.viewport_ui.w)),
                                           static_cast<std::uint32_t>(std::max(1, app.viewport_ui.h)));

        const float aspect = static_cast<float>(std::max(1, app.viewport_ui.w)) / static_cast<float>(std::max(1, app.viewport_ui.h));
        const DirectX::XMMATRIX view = app.viewport.camera.View();
        const DirectX::XMMATRIX proj = app.viewport.camera.Projection(aspect);

        aetdp1::scene::ObjectData* active_obj = app.scene.TryGet(app.selection.active_object);
        DirectX::XMFLOAT3 gizmo_origin{0.f, 0.f, 0.f};
        if (active_obj) {
            gizmo_origin = active_obj->transform.translation;
        }

        const bool draw_gizmo = (app.editor.tool_mode == aetdp1::editor::EditorToolMode::Modeling) && (active_obj != nullptr);
        const int hovered_axis = static_cast<int>(app.gizmo.hovered_axis);
        const int active_axis = static_cast<int>(app.gizmo.active_axis);

        app.renderer.RenderFrame(app.scene, app.selection, app.editor.show_selection_highlight, draw_gizmo, hovered_axis, active_axis, gizmo_origin,
                                 app.gizmo.axis_length_world, view, proj, ImGui::GetDrawData(), true);
    }

    app.renderer.WaitForGpuIdle();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    app.renderer.Destroy();
    DestroyWindow(hwnd);
    UnregisterClassW(kClassName, hinst);
    g_app = nullptr;
    return 0;
}
