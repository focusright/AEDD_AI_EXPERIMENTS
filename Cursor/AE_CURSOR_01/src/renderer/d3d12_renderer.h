#pragma once

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include <imgui.h>

struct ImDrawData;

namespace aetdp1::selection {
struct SelectionState;
}
namespace aetdp1::scene {
struct SceneData;
}

namespace aetdp1::renderer {

struct GpuVertex {
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT4 color{1.f, 1.f, 1.f, 1.f};
};

struct DescriptorHeapAllocator {
    ID3D12DescriptorHeap* heap{nullptr};
    D3D12_DESCRIPTOR_HEAP_TYPE heap_type{D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES};
    D3D12_CPU_DESCRIPTOR_HANDLE heap_start_cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE heap_start_gpu{};
    UINT handle_increment{0};
    std::vector<int> free_indices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* in_heap);
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu);
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu);
};

struct FrameContext {
    ID3D12CommandAllocator* command_allocator{nullptr};
    UINT64 fence_value{0};
};

class D3d12Renderer {
public:
    bool Create(HWND hwnd);
    void Destroy();

    void BeginResize();
    void Resize(std::uint32_t width, std::uint32_t height);

    // The embedded 3D viewport render target size (independent from the swapchain / OS client size).
    void ResizeViewportTexture(std::uint32_t width, std::uint32_t height);

    // Records viewport 3D + transitions for ImGui sampling, then renders ImGui into the swapchain.
    // Gizmo axis codes: 0 none, 1 X, 2 Y, 3 Z (mirrors `aetdp1::gizmo::GizmoAxis` integer values).
    void RenderFrame(const scene::SceneData& scene, const selection::SelectionState& selection, bool show_selection_highlight, bool draw_gizmo,
                     int hovered_axis, int active_axis, const DirectX::XMFLOAT3& gizmo_origin_world, float gizmo_axis_length_world,
                     const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection, ImDrawData* imgui_draw_data, bool vsync);

    ImTextureID ViewportSrvGpuHandle() const { return (ImTextureID)viewport_srv_gpu_.ptr; }

    ID3D12Device* device() const { return device_; }
    ID3D12CommandQueue* command_queue() const { return command_queue_; }
    ID3D12GraphicsCommandList* command_list() const { return command_list_; }
    ID3D12DescriptorHeap* srv_heap() const { return srv_heap_; }

    void AllocSrvForImGui(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) { srv_alloc_.Alloc(out_cpu, out_gpu); }
    void FreeSrvForImGui(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) { srv_alloc_.Free(cpu, gpu); }

    // Explicit GPU drain for shutdown ordering (Dear ImGui DX12 teardown vs command list lifetime).
    void WaitForGpuIdle();

private:
    static constexpr int kFramesInFlight = 2;
    static constexpr int kBackbuffers = 2;
    static constexpr int kSrvHeapSize = 128;

    HWND hwnd_{};

    ID3D12Device* device_{};
    ID3D12CommandQueue* command_queue_{};
    ID3D12GraphicsCommandList* command_list_{};

    ID3D12DescriptorHeap* rtv_heap_{};
    ID3D12DescriptorHeap* srv_heap_{};
    ID3D12DescriptorHeap* dsv_heap_{};

    DescriptorHeapAllocator srv_alloc_{};

    IDXGISwapChain3* swapchain_{};
    ID3D12Resource* backbuffers_[kBackbuffers]{};
    D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_rtv_[kBackbuffers]{};

    FrameContext frames_[kFramesInFlight]{};
    UINT frame_index_{0};

    ID3D12Fence* fence_{};
    HANDLE fence_event_{};
    UINT64 fence_last_{0};

    UINT rtv_descriptor_size_{0};
    UINT dsv_descriptor_size_{0};

    // Viewport offscreen target
    ID3D12Resource* viewport_color_{};
    ID3D12Resource* viewport_depth_{};
    D3D12_CPU_DESCRIPTOR_HANDLE viewport_rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE viewport_dsv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE viewport_srv_cpu_{};
    D3D12_GPU_DESCRIPTOR_HANDLE viewport_srv_gpu_{};

    std::uint32_t viewport_w_{4};
    std::uint32_t viewport_h_{4};
    std::uint32_t client_w_{4};
    std::uint32_t client_h_{4};

    ID3D12RootSignature* root_sig_{};
    ID3D12PipelineState* pso_tri_{};
    ID3D12PipelineState* pso_line_{};

    ID3D12Resource* mesh_vb_{};
    D3D12_VERTEX_BUFFER_VIEW mesh_vbv_{};
    UINT mesh_vertex_capacity_{0};

    bool CreateSwapchain();
    void CreateBackbuffers();
    void ReleaseBackbuffers();

    bool CreateViewportTarget(std::uint32_t w, std::uint32_t h);
    void ReleaseViewportTarget();

    bool CreateDrawPipeline();
    void ReleaseDrawPipeline();

    FrameContext* WaitForNextFrame();

    void UploadMeshVertices(const std::vector<GpuVertex>& verts);

    void RecordViewportPass(const scene::SceneData& scene, const selection::SelectionState& selection, bool show_selection_highlight, bool draw_gizmo,
                            int hovered_axis, int active_axis, const DirectX::XMFLOAT3& gizmo_origin_world, float gizmo_axis_length_world,
                            const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
};

} // namespace aetdp1::renderer
