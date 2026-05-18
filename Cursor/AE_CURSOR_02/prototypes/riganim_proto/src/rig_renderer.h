#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <cstdint>
#include <vector>

#include <DirectXMath.h>
#include <imgui.h>

struct ImDrawData;

namespace aerigp1 {

struct RigDocument;

struct GpuVertex {
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT4 color{1.f, 1.f, 1.f, 1.f};
};

class RigRenderer {
public:
    bool Create(HWND hwnd);
    void Destroy();
    void BeginResize();
    void Resize(std::uint32_t width, std::uint32_t height);
    void ResizeViewportTexture(std::uint32_t width, std::uint32_t height);
    void RenderFrame(const RigDocument& doc, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection, ImDrawData* imgui_draw_data, bool vsync);
    ImTextureID ViewportSrvGpuHandle() const { return (ImTextureID)viewport_srv_gpu_.ptr; }
    ID3D12Device* device() const { return device_; }
    ID3D12CommandQueue* command_queue() const { return command_queue_; }
    ID3D12DescriptorHeap* srv_heap() const { return srv_heap_; }
    void AllocSrvForImGui(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu);
    void FreeSrvForImGui(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu);
    void WaitForGpuIdle();

private:
    static constexpr int kFramesInFlight = 2;
    static constexpr int kBackbuffers = 2;
    static constexpr int kSrvHeapSize = 128;
    static constexpr UINT kMaxMeshVertices = 200000;

    HWND hwnd_{};
    ID3D12Device* device_{};
    ID3D12CommandQueue* command_queue_{};
    ID3D12GraphicsCommandList* command_list_{};
    ID3D12DescriptorHeap* rtv_heap_{};
    ID3D12DescriptorHeap* srv_heap_{};
    ID3D12DescriptorHeap* dsv_heap_{};
    IDXGISwapChain3* swapchain_{};
    ID3D12Resource* backbuffers_[kBackbuffers]{};
    D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_rtv_[kBackbuffers]{};
    struct FrameContext {
        ID3D12CommandAllocator* command_allocator{};
        UINT64 fence_value{};
    } frames_[kFramesInFlight]{};
    UINT frame_index_{0};
    ID3D12Fence* fence_{};
    HANDLE fence_event_{};
    UINT64 fence_last_{0};
    UINT rtv_descriptor_size_{0};
    ID3D12Resource* viewport_color_{};
    ID3D12Resource* viewport_depth_{};
    D3D12_CPU_DESCRIPTOR_HANDLE viewport_rtv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE viewport_dsv_{};
    D3D12_CPU_DESCRIPTOR_HANDLE viewport_srv_cpu_{};
    D3D12_GPU_DESCRIPTOR_HANDLE viewport_srv_gpu_{};
    std::uint32_t viewport_w_{4}, viewport_h_{4}, client_w_{4}, client_h_{4};
    ID3D12RootSignature* root_sig_{};
    ID3D12PipelineState* pso_tri_{};
    ID3D12PipelineState* pso_line_{};
    ID3D12Resource* mesh_vb_{};
    D3D12_VERTEX_BUFFER_VIEW mesh_vbv_{};
    UINT mesh_vertex_capacity_{0};
    std::vector<int> srv_free_;

    bool CreateSwapchain();
    void CreateBackbuffers();
    void ReleaseBackbuffers();
    bool CreateViewportTarget(std::uint32_t w, std::uint32_t h);
    void ReleaseViewportTarget();
    bool CreateDrawPipeline();
    FrameContext* WaitForNextFrame();
    void UploadVertices(const std::vector<GpuVertex>& verts);
    void BuildSceneVerts(const RigDocument& doc, std::vector<GpuVertex>& tris, std::vector<GpuVertex>& lines);
};

} // namespace aerigp1
