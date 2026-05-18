#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "procedural_character.h"
#include "rig_serializer.h"
#include "rig_ui.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
aerigp1::AppState* g_app = nullptr;

static void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) {
    auto* r = reinterpret_cast<aerigp1::RigRenderer*>(info->UserData);
    r->AllocSrvForImGui(out_cpu, out_gpu);
}

static void ImGuiSrvFree(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    auto* r = reinterpret_cast<aerigp1::RigRenderer*>(info->UserData);
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
            g_app->renderer.BeginResize();
            g_app->renderer.Resize(static_cast<std::uint32_t>(std::max(1L, cr.right - cr.left)),
                                   static_cast<std::uint32_t>(std::max(1L, cr.bottom - cr.top)));
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

static int ExportBundledSamples() {
    aerigp1::AppState app{};
    wchar_t exe_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::filesystem::path probe = std::filesystem::path(exe_path).parent_path();
    for (int i = 0; i < 8 && !probe.empty(); ++i) {
        if (std::filesystem::exists(probe / "AERIGP1.sln")) {
            app.data_dir = probe / "data";
            break;
        }
        probe = probe.parent_path();
    }
    if (app.data_dir.empty()) {
        return 1;
    }
    std::filesystem::create_directories(app.data_dir);
    aerigp1::BuildDefaultHumanoid(app.doc);
    const bool rig_ok = aerigp1::RigSerializer::SaveRig(app.doc, (app.data_dir / "sample_character.aerig").string());
    const bool anim_ok = aerigp1::RigSerializer::SaveAnim(app.doc, app.doc.active_clip, (app.data_dir / "sample_wave.aeanim").string());
    return (rig_ok && anim_ok) ? 0 : 2;
}

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR cmdline, int) {
    if (wcsstr(cmdline, L"--export-samples")) {
        return ExportBundledSamples();
    }
    const wchar_t kClass[] = L"AERIGP1_WND";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, kClass, L"AERIGP1 — Another Engine Rig/Animation Prototype 1", WS_OVERLAPPEDWINDOW, 60, 60, 1500, 900, nullptr,
                                nullptr, hinst, nullptr);
    if (!hwnd) {
        return 1;
    }

    aerigp1::AppState app{};
    g_app = &app;
    app.hwnd = hwnd;
    {
        wchar_t exe_path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        std::filesystem::path probe = std::filesystem::path(exe_path).parent_path();
        for (int i = 0; i < 8 && !probe.empty(); ++i) {
            if (std::filesystem::exists(probe / "AERIGP1.sln")) {
                app.data_dir = probe / "data";
                break;
            }
            probe = probe.parent_path();
        }
        if (app.data_dir.empty()) {
            app.data_dir = std::filesystem::path(exe_path).parent_path() / "data";
        }
    }
    if (!app.renderer.Create(hwnd)) {
        MessageBoxW(hwnd, L"D3D12 init failed", L"AERIGP1", MB_ICONERROR);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo init_info{};
    init_info.Device = app.renderer.device();
    init_info.CommandQueue = app.renderer.command_queue();
    init_info.NumFramesInFlight = 2;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    init_info.SrvDescriptorHeap = app.renderer.srv_heap();
    init_info.SrvDescriptorAllocFn = ImGuiSrvAlloc;
    init_info.SrvDescriptorFreeFn = ImGuiSrvFree;
    init_info.UserData = &app.renderer;
    ImGui_ImplDX12_Init(&init_info);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    aerigp1::RigUiInit(app);

    auto last = std::chrono::steady_clock::now();
    MSG msg{};
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        aerigp1::RigUiFrame(app, dt);

        using namespace DirectX;
        const XMMATRIX view =
            XMMatrixLookAtLH(XMVectorAdd(XMLoadFloat3(&app.cam_target), XMVectorSet(app.cam_dist * sinf(app.cam_yaw) * cosf(app.cam_pitch),
                                                                                     app.cam_dist * sinf(app.cam_pitch), app.cam_dist * cosf(app.cam_yaw) * cosf(app.cam_pitch), 0.f)),
                             XMLoadFloat3(&app.cam_target), XMVectorSet(0.f, 1.f, 0.f, 0.f));
        const float aspect = static_cast<float>(app.viewport_w) / static_cast<float>(std::max(1, app.viewport_h));
        const XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(50.f), aspect, 0.1f, 100.f);

        ImGui::Render();
        app.renderer.RenderFrame(app.doc, view, proj, ImGui::GetDrawData(), true);
    }

    app.renderer.WaitForGpuIdle();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    app.renderer.Destroy();
    return 0;
}
