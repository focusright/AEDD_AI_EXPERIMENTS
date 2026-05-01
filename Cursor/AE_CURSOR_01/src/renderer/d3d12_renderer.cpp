#include "d3d12_renderer.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <d3dcompiler.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <wrl/client.h>

#include "core/types.h"
#include "scene/scene_data.h"
#include "selection/selection_state.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace aetdp1::renderer {
namespace {

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static constexpr char kShader[] = R"(
struct VSInput {
    float3 pos : POSITION;
    float4 col : COLOR;
};
struct PSInput {
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};
cbuffer PushConstants : register(b0) {
    float4x4 MVP;
};
PSInput VSMain(VSInput input) {
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), MVP);
    output.col = input.col;
    return output;
}
float4 PSMain(PSInput input) : SV_TARGET {
    return input.col;
}
)";

static bool HrOk(HRESULT hr) { return SUCCEEDED(hr); }

static bool CreateDeviceWithFallback(IDXGIFactory4* factory, ID3D12Device** out_device) {
    if (!factory || !out_device) {
        return false;
    }

    *out_device = nullptr;

    // 1) Prefer a hardware adapter (non-software) that can create a D3D12 device.
    for (UINT adapter_index = 0;; ++adapter_index) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(adapter_index, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (!HrOk(adapter->GetDesc1(&desc))) {
            continue;
        }
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        if (HrOk(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(out_device)))) {
            return true;
        }
    }

    // 2) Fallback to WARP software adapter so the prototype can still run on systems without
    // a usable hardware D3D12 path (e.g. VM / remote sessions).
    ComPtr<IDXGIAdapter> warp_adapter;
    if (!HrOk(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)))) {
        return false;
    }

    return HrOk(D3D12CreateDevice(warp_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(out_device)));
}

static D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES p{};
    p.Type = type;
    p.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    p.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    p.CreationNodeMask = 1;
    p.VisibleNodeMask = 1;
    return p;
}

static D3D12_RESOURCE_DESC BufferDesc(UINT64 byte_size) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Alignment = 0;
    d.Width = byte_size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count = 1;
    d.SampleDesc.Quality = 0;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d.Flags = D3D12_RESOURCE_FLAG_NONE;
    return d;
}

static D3D12_RESOURCE_DESC Tex2D(DXGI_FORMAT format, UINT w, UINT h, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Alignment = 0;
    d.Width = w;
    d.Height = h;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.SampleDesc.Quality = 0;
    d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d.Flags = flags;
    return d;
}

static D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    return b;
}

static void AppendBoxTriangles(const scene::ObjectData& obj, std::vector<GpuVertex>& out) {
    const XMMATRIX m = TransformToMatrix(obj.transform);

    auto push_tri = [&](XMVECTOR a, XMVECTOR b, XMVECTOR c, XMFLOAT4 col) {
        GpuVertex v0{}, v1{}, v2{};
        XMStoreFloat3(&v0.position, a);
        XMStoreFloat3(&v1.position, b);
        XMStoreFloat3(&v2.position, c);
        v0.color = col;
        v1.color = col;
        v2.color = col;
        out.push_back(v0);
        out.push_back(v1);
        out.push_back(v2);
    };

    const auto corner = [&](int i) { return XMVector3Transform(XMLoadFloat3(&obj.local_box.v[i]), m); };

    const int faces[12][3] = {{0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 4, 5}, {0, 5, 1},
                              {2, 6, 7}, {2, 7, 3}, {0, 3, 7}, {0, 7, 4}, {1, 5, 6}, {1, 6, 2}};
    const int tri_to_face[12] = {
        0, 0, // -Y
        1, 1, // +Y
        2, 2, // -Z
        3, 3, // +Z
        4, 4, // -X
        5, 5  // +X
    };

    for (int f = 0; f < 12; ++f) {
        const XMVECTOR a = corner(faces[f][0]);
        const XMVECTOR b = corner(faces[f][1]);
        const XMVECTOR c = corner(faces[f][2]);
        const auto& fc = obj.face_colors.rgb[static_cast<std::size_t>(tri_to_face[f])];
        push_tri(a, b, c, XMFLOAT4(fc.x, fc.y, fc.z, 1.0f));
    }
}

static XMFLOAT4 AxisColor(int axis, int hovered, int active) {
    const bool on = (axis == active) || (axis == hovered && active == 0);
    const float s = on ? 1.f : 0.55f;
    if (axis == 1) {
        return {s, 0.15f, 0.15f, 1.f};
    }
    if (axis == 2) {
        return {0.15f, s, 0.15f, 1.f};
    }
    if (axis == 3) {
        return {0.15f, 0.25f, s, 1.f};
    }
    return {1.f, 1.f, 1.f, 1.f};
}

static void AppendTranslateGizmo(const XMFLOAT3& origin, float axis_len, int hovered_axis, int active_axis, std::vector<GpuVertex>& out) {
    const XMVECTOR o = XMLoadFloat3(&origin);
    const XMVECTOR x = XMVectorAdd(o, XMVectorScale(XMVectorSet(1.f, 0.f, 0.f, 0.f), axis_len));
    const XMVECTOR y = XMVectorAdd(o, XMVectorScale(XMVectorSet(0.f, 1.f, 0.f, 0.f), axis_len));
    const XMVECTOR z = XMVectorAdd(o, XMVectorScale(XMVectorSet(0.f, 0.f, 1.f, 0.f), axis_len));

    auto push_line = [&](XMVECTOR a, XMVECTOR b, const XMFLOAT4& col) {
        GpuVertex va{}, vb{};
        XMStoreFloat3(&va.position, a);
        XMStoreFloat3(&vb.position, b);
        va.color = col;
        vb.color = col;
        out.push_back(va);
        out.push_back(vb);
    };

    push_line(o, x, AxisColor(1, hovered_axis, active_axis));
    push_line(o, y, AxisColor(2, hovered_axis, active_axis));
    push_line(o, z, AxisColor(3, hovered_axis, active_axis));
}

} // namespace

void DescriptorHeapAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* in_heap) {
    heap = in_heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    heap_type = desc.Type;
    heap_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
    heap_start_gpu = heap->GetGPUDescriptorHandleForHeapStart();
    handle_increment = device->GetDescriptorHandleIncrementSize(heap_type);
    free_indices.clear();
    free_indices.reserve(static_cast<int>(desc.NumDescriptors));
    for (int n = static_cast<int>(desc.NumDescriptors); n > 0; --n) {
        free_indices.push_back(n - 1);
    }
}

void DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) {
    IM_ASSERT(!free_indices.empty());
    const int idx = free_indices.back();
    free_indices.pop_back();
    out_cpu->ptr = heap_start_cpu.ptr + (static_cast<SIZE_T>(idx) * handle_increment);
    out_gpu->ptr = heap_start_gpu.ptr + (static_cast<SIZE_T>(idx) * handle_increment);
}

void DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    const int cpu_idx = static_cast<int>((cpu.ptr - heap_start_cpu.ptr) / handle_increment);
    const int gpu_idx = static_cast<int>((gpu.ptr - heap_start_gpu.ptr) / handle_increment);
    IM_ASSERT(cpu_idx == gpu_idx);
    free_indices.push_back(cpu_idx);
}

bool D3d12Renderer::Create(HWND hwnd) {
    hwnd_ = hwnd;

    UINT dxgi_factory_flags = 0;
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (HrOk(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            debug_controller->EnableDebugLayer();
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    if (!HrOk(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)))) {
        return false;
    }

    if (!CreateDeviceWithFallback(factory.Get(), &device_)) {
        return false;
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kBackbuffers + 4;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (!HrOk(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtv_heap_)))) {
            return false;
        }
        rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE h = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        for (int i = 0; i < kBackbuffers; ++i) {
            backbuffer_rtv_[i] = h;
            h.ptr += rtv_descriptor_size_;
        }
        viewport_rtv_ = h;
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = kSrvHeapSize;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (!HrOk(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srv_heap_)))) {
            return false;
        }
        srv_alloc_.Create(device_, srv_heap_);
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 4;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (!HrOk(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsv_heap_)))) {
            return false;
        }
        dsv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        viewport_dsv_ = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    }

    {
        D3D12_COMMAND_QUEUE_DESC q{};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (!HrOk(device_->CreateCommandQueue(&q, IID_PPV_ARGS(&command_queue_)))) {
            return false;
        }
    }

    for (int i = 0; i < kFramesInFlight; ++i) {
        if (!HrOk(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frames_[i].command_allocator)))) {
            return false;
        }
    }

    if (!HrOk(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].command_allocator, nullptr, IID_PPV_ARGS(&command_list_)))) {
        return false;
    }
    if (!HrOk(command_list_->Close())) {
        return false;
    }

    if (!HrOk(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) {
        return false;
    }
    fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event_) {
        return false;
    }

    if (!CreateSwapchain()) {
        return false;
    }

    RECT cr{};
    GetClientRect(hwnd_, &cr);
    client_w_ = static_cast<std::uint32_t>(std::max(1L, cr.right - cr.left));
    client_h_ = static_cast<std::uint32_t>(std::max(1L, cr.bottom - cr.top));
    viewport_w_ = std::max(4u, std::min(client_w_, 960u));
    viewport_h_ = std::max(4u, std::min(client_h_, 720u));

    if (!CreateViewportTarget(viewport_w_, viewport_h_)) {
        return false;
    }
    if (!CreateDrawPipeline()) {
        return false;
    }

    mesh_vertex_capacity_ = 200000;
    {
        const D3D12_HEAP_PROPERTIES upload_heap = HeapProps(D3D12_HEAP_TYPE_UPLOAD);
        const auto buffer_desc = BufferDesc(static_cast<UINT64>(sizeof(GpuVertex)) * mesh_vertex_capacity_);
        if (!HrOk(device_->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                   IID_PPV_ARGS(&mesh_vb_)))) {
            return false;
        }
        mesh_vbv_.BufferLocation = mesh_vb_->GetGPUVirtualAddress();
        mesh_vbv_.StrideInBytes = sizeof(GpuVertex);
        mesh_vbv_.SizeInBytes = sizeof(GpuVertex) * mesh_vertex_capacity_;
    }

    return true;
}

bool D3d12Renderer::CreateSwapchain() {
    ComPtr<IDXGIFactory4> factory;
    if (!HrOk(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.BufferCount = kBackbuffers;
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Scaling = DXGI_SCALING_STRETCH;

    ComPtr<IDXGISwapChain1> sc1;
    if (!HrOk(factory->CreateSwapChainForHwnd(command_queue_, hwnd_, &sd, nullptr, nullptr, &sc1))) {
        return false;
    }
    if (!HrOk(sc1->QueryInterface(IID_PPV_ARGS(&swapchain_)))) {
        return false;
    }
    if (!HrOk(factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER))) {
        return false;
    }

    CreateBackbuffers();
    return true;
}

void D3d12Renderer::CreateBackbuffers() {
    for (UINT i = 0; i < kBackbuffers; ++i) {
        ComPtr<ID3D12Resource> bb;
        IM_ASSERT(HrOk(swapchain_->GetBuffer(i, IID_PPV_ARGS(&bb))));
        device_->CreateRenderTargetView(bb.Get(), nullptr, backbuffer_rtv_[i]);
        backbuffers_[i] = bb.Detach();
    }
}

void D3d12Renderer::ReleaseBackbuffers() {
    for (UINT i = 0; i < kBackbuffers; ++i) {
        if (backbuffers_[i]) {
            backbuffers_[i]->Release();
            backbuffers_[i] = nullptr;
        }
    }
}

void D3d12Renderer::BeginResize() {
    WaitForGpuIdle();
    ReleaseBackbuffers();
}

void D3d12Renderer::Resize(std::uint32_t width, std::uint32_t height) {
    client_w_ = std::max(1u, width);
    client_h_ = std::max(1u, height);

    if (swapchain_) {
        IM_ASSERT(HrOk(swapchain_->ResizeBuffers(0, client_w_, client_h_, DXGI_FORMAT_UNKNOWN, 0)));
        CreateBackbuffers();
    }
}

void D3d12Renderer::ResizeViewportTexture(std::uint32_t width, std::uint32_t height) {
    const std::uint32_t w = std::max(4u, width);
    const std::uint32_t h = std::max(4u, height);
    if (w == viewport_w_ && h == viewport_h_ && viewport_color_) {
        return;
    }
    viewport_w_ = w;
    viewport_h_ = h;
    WaitForGpuIdle();
    IM_ASSERT(CreateViewportTarget(viewport_w_, viewport_h_));
}

bool D3d12Renderer::CreateViewportTarget(std::uint32_t w, std::uint32_t h) {
    ReleaseViewportTarget();

    {
        const D3D12_HEAP_PROPERTIES default_heap = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
        auto color_desc = Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, w, h, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        const float clear_color[4] = {0.10f, 0.10f, 0.12f, 1.f};
        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        memcpy(clear_value.Color, clear_color, sizeof(clear_color));
        if (!HrOk(device_->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &color_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                    &clear_value, IID_PPV_ARGS(&viewport_color_)))) {
            return false;
        }

        device_->CreateRenderTargetView(viewport_color_, nullptr, viewport_rtv_);

        srv_alloc_.Alloc(&viewport_srv_cpu_, &viewport_srv_gpu_);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        srv.Texture2D.MostDetailedMip = 0;
        srv.Texture2D.PlaneSlice = 0;
        srv.Texture2D.ResourceMinLODClamp = 0.f;
        device_->CreateShaderResourceView(viewport_color_, &srv, viewport_srv_cpu_);
    }

    {
        const D3D12_HEAP_PROPERTIES default_heap = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
        auto ds_desc = Tex2D(DXGI_FORMAT_D32_FLOAT, w, h, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        D3D12_CLEAR_VALUE ds_clear{};
        ds_clear.Format = DXGI_FORMAT_D32_FLOAT;
        ds_clear.DepthStencil.Depth = 1.f;
        ds_clear.DepthStencil.Stencil = 0;
        if (!HrOk(device_->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &ds_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ds_clear,
                                                    IID_PPV_ARGS(&viewport_depth_)))) {
            return false;
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Flags = D3D12_DSV_FLAG_NONE;
        dsv.Texture2D.MipSlice = 0;
        device_->CreateDepthStencilView(viewport_depth_, &dsv, viewport_dsv_);
    }

    return true;
}

void D3d12Renderer::ReleaseViewportTarget() {
    if (viewport_color_) {
        if (viewport_srv_cpu_.ptr != 0) {
            srv_alloc_.Free(viewport_srv_cpu_, viewport_srv_gpu_);
            viewport_srv_cpu_.ptr = 0;
            viewport_srv_gpu_.ptr = 0;
        }
        viewport_color_->Release();
        viewport_color_ = nullptr;
    }
    if (viewport_depth_) {
        viewport_depth_->Release();
        viewport_depth_ = nullptr;
    }
}

bool D3d12Renderer::CreateDrawPipeline() {
    ComPtr<ID3DBlob> vs_blob;
    ComPtr<ID3DBlob> ps_blob;
    ComPtr<ID3DBlob> err_blob;
    const UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    if (!HrOk(D3DCompile(kShader, static_cast<UINT>(strlen(kShader)), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compile_flags, 0,
                     &vs_blob, &err_blob))) {
        return false;
    }
    if (!HrOk(D3DCompile(kShader, static_cast<UINT>(strlen(kShader)), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compile_flags, 0,
                     &ps_blob, &err_blob))) {
        return false;
    }

    D3D12_ROOT_PARAMETER root_param{};
    root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    root_param.Constants.ShaderRegister = 0;
    root_param.Constants.RegisterSpace = 0;
    root_param.Constants.Num32BitValues = 16;

    D3D12_ROOT_SIGNATURE_DESC rs_desc{};
    rs_desc.NumParameters = 1;
    rs_desc.pParameters = &root_param;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rs_blob;
    ComPtr<ID3DBlob> rs_err;
    if (!HrOk(D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &rs_blob, &rs_err))) {
        return false;
    }
    if (!HrOk(device_->CreateRootSignature(0, rs_blob->GetBufferPointer(), rs_blob->GetBufferSize(), IID_PPV_ARGS(&root_sig_)))) {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC il[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = {il, _countof(il)};
    pso.pRootSignature = root_sig_;
    pso.VS = {vs_blob->GetBufferPointer(), vs_blob->GetBufferSize()};
    pso.PS = {ps_blob->GetBufferPointer(), ps_blob->GetBufferSize()};
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    pso.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pso.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.RasterizerState.MultisampleEnable = FALSE;
    pso.RasterizerState.AntialiasedLineEnable = FALSE;
    pso.RasterizerState.ForcedSampleCount = 0;
    pso.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    pso.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.DepthStencilState.StencilEnable = FALSE;

    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;

    if (!HrOk(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_tri_)))) {
        return false;
    }

    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    if (!HrOk(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_line_)))) {
        return false;
    }

    return true;
}

void D3d12Renderer::ReleaseDrawPipeline() {
    if (pso_tri_) {
        pso_tri_->Release();
        pso_tri_ = nullptr;
    }
    if (pso_line_) {
        pso_line_->Release();
        pso_line_ = nullptr;
    }
    if (root_sig_) {
        root_sig_->Release();
        root_sig_ = nullptr;
    }
}

void D3d12Renderer::WaitForGpuIdle() {
    if (!command_queue_ || !fence_) {
        return;
    }
    const UINT64 v = ++fence_last_;
    IM_ASSERT(HrOk(command_queue_->Signal(fence_, v)));
    if (fence_->GetCompletedValue() < v) {
        IM_ASSERT(HrOk(fence_->SetEventOnCompletion(v, fence_event_)));
        WaitForSingleObject(fence_event_, INFINITE);
    }
}

FrameContext* D3d12Renderer::WaitForNextFrame() {
    FrameContext* fc = &frames_[frame_index_ % kFramesInFlight];
    if (fence_->GetCompletedValue() < fc->fence_value) {
        IM_ASSERT(HrOk(fence_->SetEventOnCompletion(fc->fence_value, fence_event_)));
        WaitForSingleObject(fence_event_, INFINITE);
    }
    return fc;
}

void D3d12Renderer::UploadMeshVertices(const std::vector<GpuVertex>& verts) {
    if (verts.empty()) {
        return;
    }
    IM_ASSERT(static_cast<int>(verts.size()) <= mesh_vertex_capacity_);
    void* mapped = nullptr;
    const D3D12_RANGE read_range{0, 0};
    IM_ASSERT(HrOk(mesh_vb_->Map(0, &read_range, &mapped)));
    memcpy(mapped, verts.data(), sizeof(GpuVertex) * verts.size());
    mesh_vb_->Unmap(0, nullptr);
    mesh_vbv_.SizeInBytes = sizeof(GpuVertex) * static_cast<UINT>(verts.size());
}

void D3d12Renderer::RecordViewportPass(const scene::SceneData& scene, const selection::SelectionState& selection, bool show_selection_highlight,
                                       bool draw_gizmo, int hovered_axis, int active_axis, const XMFLOAT3& gizmo_origin_world, float gizmo_axis_length_world,
                                       const XMMATRIX& view, const XMMATRIX& projection) {
    std::vector<GpuVertex> verts;
    verts.reserve(scene.objects.size() * 36 + 64);

    for (const auto& obj : scene.objects) {
        scene::ObjectData draw_obj = obj;
        if (show_selection_highlight && selection.IsSelected(obj.id)) {
            for (auto& fc : draw_obj.face_colors.rgb) {
                fc.x = std::min(1.f, fc.x + 0.18f);
                fc.y = std::min(1.f, fc.y + 0.18f);
                fc.z = std::min(1.f, fc.z + 0.18f);
            }
        }
        AppendBoxTriangles(draw_obj, verts);
    }

    if (draw_gizmo) {
        AppendTranslateGizmo(gizmo_origin_world, gizmo_axis_length_world, hovered_axis, active_axis, verts);
    }

    UploadMeshVertices(verts);

    D3D12_RESOURCE_BARRIER barrier_to_rtv =
        TransitionBarrier(viewport_color_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_RESOURCE_BARRIER barrier_to_srv =
        TransitionBarrier(viewport_color_, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    command_list_->ResourceBarrier(1, &barrier_to_rtv);

    const float clear_color[4] = {0.08f, 0.08f, 0.10f, 1.f};
    command_list_->ClearRenderTargetView(viewport_rtv_, clear_color, 0, nullptr);
    command_list_->ClearDepthStencilView(viewport_dsv_, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    D3D12_VIEWPORT vp{};
    vp.Width = static_cast<float>(viewport_w_);
    vp.Height = static_cast<float>(viewport_h_);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    D3D12_RECT sc{};
    sc.left = 0;
    sc.top = 0;
    sc.right = static_cast<LONG>(viewport_w_);
    sc.bottom = static_cast<LONG>(viewport_h_);
    command_list_->RSSetViewports(1, &vp);
    command_list_->RSSetScissorRects(1, &sc);

    command_list_->OMSetRenderTargets(1, &viewport_rtv_, FALSE, &viewport_dsv_);

    ID3D12DescriptorHeap* heaps[] = {srv_heap_};
    command_list_->SetDescriptorHeaps(1, heaps);

    command_list_->SetGraphicsRootSignature(root_sig_);

    const XMMATRIX vp_mat = view * projection;

    const std::size_t verts_per_object = 36;
    std::size_t tri_vert_count = verts.size();
    if (draw_gizmo && tri_vert_count >= 6) {
        tri_vert_count -= 6;
    }

    command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list_->SetPipelineState(pso_tri_);

    for (std::size_t oi = 0; oi < scene.objects.size(); ++oi) {
        const XMMATRIX mvp = XMMatrixTranspose(vp_mat);
        XMFLOAT4X4 mvp_store{};
        XMStoreFloat4x4(&mvp_store, mvp);
        command_list_->SetGraphicsRoot32BitConstants(0, 16, &mvp_store, 0);

        const UINT draw_count = static_cast<UINT>(verts_per_object);
        const UINT64 byte_offset = static_cast<UINT64>(oi * verts_per_object * sizeof(GpuVertex));
        mesh_vbv_.BufferLocation = mesh_vb_->GetGPUVirtualAddress() + byte_offset;
        mesh_vbv_.SizeInBytes = sizeof(GpuVertex) * draw_count;
        command_list_->IASetVertexBuffers(0, 1, &mesh_vbv_);
        command_list_->DrawInstanced(draw_count, 1, 0, 0);
    }

    if (draw_gizmo && verts.size() >= 6) {
        const XMMATRIX mvp = XMMatrixTranspose(vp_mat);
        XMFLOAT4X4 mvp_store{};
        XMStoreFloat4x4(&mvp_store, mvp);
        command_list_->SetGraphicsRoot32BitConstants(0, 16, &mvp_store, 0);

        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        command_list_->SetPipelineState(pso_line_);
        mesh_vbv_.BufferLocation = mesh_vb_->GetGPUVirtualAddress() + static_cast<UINT64>(tri_vert_count * sizeof(GpuVertex));
        mesh_vbv_.SizeInBytes = sizeof(GpuVertex) * 6;
        command_list_->IASetVertexBuffers(0, 1, &mesh_vbv_);
        command_list_->DrawInstanced(6, 1, 0, 0);
    }

    command_list_->ResourceBarrier(1, &barrier_to_srv);
}

void D3d12Renderer::RenderFrame(const scene::SceneData& scene, const selection::SelectionState& selection, bool show_selection_highlight, bool draw_gizmo,
                                int hovered_axis, int active_axis, const XMFLOAT3& gizmo_origin_world, float gizmo_axis_length_world,
                                const XMMATRIX& view, const XMMATRIX& projection, ImDrawData* imgui_draw_data, bool vsync) {
    FrameContext* fc = WaitForNextFrame();
    const UINT back_buffer_idx = swapchain_->GetCurrentBackBufferIndex();

    IM_ASSERT(HrOk(fc->command_allocator->Reset()));
    IM_ASSERT(HrOk(command_list_->Reset(fc->command_allocator, nullptr)));

    RecordViewportPass(scene, selection, show_selection_highlight, draw_gizmo, hovered_axis, active_axis, gizmo_origin_world, gizmo_axis_length_world,
                       view, projection);

    D3D12_RESOURCE_BARRIER to_rtv =
        TransitionBarrier(backbuffers_[back_buffer_idx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    command_list_->ResourceBarrier(1, &to_rtv);

    const float ui_clear[4] = {0.12f, 0.12f, 0.12f, 1.f};
    command_list_->ClearRenderTargetView(backbuffer_rtv_[back_buffer_idx], ui_clear, 0, nullptr);
    command_list_->OMSetRenderTargets(1, &backbuffer_rtv_[back_buffer_idx], FALSE, nullptr);

    D3D12_VIEWPORT vp{};
    vp.Width = static_cast<float>(client_w_);
    vp.Height = static_cast<float>(client_h_);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    D3D12_RECT sc{};
    sc.left = 0;
    sc.top = 0;
    sc.right = static_cast<LONG>(client_w_);
    sc.bottom = static_cast<LONG>(client_h_);
    command_list_->RSSetViewports(1, &vp);
    command_list_->RSSetScissorRects(1, &sc);

    ID3D12DescriptorHeap* heaps[] = {srv_heap_};
    command_list_->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(imgui_draw_data, command_list_);

    D3D12_RESOURCE_BARRIER to_present =
        TransitionBarrier(backbuffers_[back_buffer_idx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    command_list_->ResourceBarrier(1, &to_present);

    IM_ASSERT(HrOk(command_list_->Close()));

    ID3D12CommandList* lists[] = {command_list_};
    command_queue_->ExecuteCommandLists(1, lists);

    const UINT64 fence_value = ++fence_last_;
    IM_ASSERT(HrOk(command_queue_->Signal(fence_, fence_value)));
    fc->fence_value = fence_value;

    IM_ASSERT(HrOk(swapchain_->Present(vsync ? 1 : 0, 0)));
    frame_index_++;
}

void D3d12Renderer::Destroy() {
    WaitForGpuIdle();

    ReleaseDrawPipeline();

    if (mesh_vb_) {
        mesh_vb_->Release();
        mesh_vb_ = nullptr;
    }

    ReleaseViewportTarget();
    BeginResize();
    if (swapchain_) {
        swapchain_->Release();
        swapchain_ = nullptr;
    }

    for (int i = 0; i < kFramesInFlight; ++i) {
        if (frames_[i].command_allocator) {
            frames_[i].command_allocator->Release();
            frames_[i].command_allocator = nullptr;
        }
    }

    if (command_list_) {
        command_list_->Release();
        command_list_ = nullptr;
    }
    if (command_queue_) {
        command_queue_->Release();
        command_queue_ = nullptr;
    }

    if (rtv_heap_) {
        rtv_heap_->Release();
        rtv_heap_ = nullptr;
    }
    if (srv_heap_) {
        srv_heap_->Release();
        srv_heap_ = nullptr;
    }
    if (dsv_heap_) {
        dsv_heap_->Release();
        dsv_heap_ = nullptr;
    }

    if (fence_) {
        fence_->Release();
        fence_ = nullptr;
    }
    if (fence_event_) {
        CloseHandle(fence_event_);
        fence_event_ = nullptr;
    }

    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
}

} // namespace aetdp1::renderer
