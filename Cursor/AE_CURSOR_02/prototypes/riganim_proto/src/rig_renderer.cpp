#include "rig_renderer.h"

#include <cstring>
#include <imgui_impl_dx12.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include "rig_types.h"
#include "skeleton.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace aerigp1 {
namespace {
using Microsoft::WRL::ComPtr;
using namespace DirectX;

static constexpr char kShader[] = R"(
struct VSInput { float3 pos : POSITION; float4 col : COLOR; };
struct PSInput { float4 pos : SV_POSITION; float4 col : COLOR; };
cbuffer PushConstants : register(b0) { float4x4 MVP; };
PSInput VSMain(VSInput input) {
    PSInput o;
    o.pos = mul(float4(input.pos, 1.0f), MVP);
    o.col = input.col;
    return o;
}
float4 PSMain(PSInput input) : SV_TARGET { return input.col; }
)";

static bool HrOk(HRESULT hr) { return SUCCEEDED(hr); }

static D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES p{};
    p.Type = type;
    return p;
}

static D3D12_RESOURCE_DESC BufferDesc(UINT64 size) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Alignment = 0;
    d.Width = size;
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

static UINT64 AlignBufferSize(UINT64 size) {
    const UINT64 align = 256;
    return (size + align - 1) & ~(align - 1);
}

static D3D12_RESOURCE_DESC Tex2D(DXGI_FORMAT format, UINT w, UINT h, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = w;
    d.Height = h;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d.Flags = flags;
    return d;
}

static bool CreateDeviceWithFallback(IDXGIFactory4* factory, ID3D12Device** out_device) {
    if (!factory || !out_device) {
        return false;
    }
    *out_device = nullptr;
    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
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
    ComPtr<IDXGIAdapter> warp;
    if (!HrOk(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))) {
        return false;
    }
    return HrOk(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(out_device)));
}

static D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return b;
}

static void PushTri(std::vector<GpuVertex>& out, XMVECTOR a, XMVECTOR b, XMVECTOR c, XMFLOAT4 col) {
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
}

static void PushLine(std::vector<GpuVertex>& out, XMVECTOR a, XMVECTOR b, XMFLOAT4 col) {
    GpuVertex v0{}, v1{};
    XMStoreFloat3(&v0.position, a);
    XMStoreFloat3(&v1.position, b);
    v0.color = col;
    v1.color = col;
    out.push_back(v0);
    out.push_back(v1);
}

static XMFLOAT4 JointColor(const Joint& j, bool selected) {
    const std::uint32_t c = j.display_color;
    XMFLOAT4 col{static_cast<float>((c >> 16) & 0xFF) / 255.f, static_cast<float>((c >> 8) & 0xFF) / 255.f, static_cast<float>(c & 0xFF) / 255.f, 1.f};
    if (selected) {
        col.x = std::min(1.f, col.x + 0.35f);
        col.y = std::min(1.f, col.y + 0.35f);
        col.z = std::min(1.f, col.z + 0.35f);
    }
    return col;
}

} // namespace

void RigRenderer::AllocSrvForImGui(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) {
    if (srv_free_.empty()) {
        return;
    }
    const int idx = srv_free_.back();
    srv_free_.pop_back();
    const UINT inc = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE start = srv_heap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gstart = srv_heap_->GetGPUDescriptorHandleForHeapStart();
    out_cpu->ptr = start.ptr + static_cast<SIZE_T>(idx) * inc;
    out_gpu->ptr = gstart.ptr + static_cast<SIZE_T>(idx) * inc;
}

void RigRenderer::FreeSrvForImGui(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    const UINT inc = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE start = srv_heap_->GetCPUDescriptorHandleForHeapStart();
    const int idx = static_cast<int>((cpu.ptr - start.ptr) / inc);
    (void)gpu;
    srv_free_.push_back(idx);
}

bool RigRenderer::Create(HWND hwnd) {
    hwnd_ = hwnd;
    ComPtr<IDXGIFactory4> factory;
    if (!HrOk(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return false;
    }
    if (!CreateDeviceWithFallback(factory.Get(), &device_)) {
        return false;
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kBackbuffers + 2;
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
        srv_free_.reserve(kSrvHeapSize);
        for (int i = kSrvHeapSize - 1; i >= 0; --i) {
            srv_free_.push_back(i);
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 2;
        if (!HrOk(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsv_heap_)))) {
            return false;
        }
        viewport_dsv_ = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    }

    {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (!HrOk(device_->CreateCommandQueue(&qd, IID_PPV_ARGS(&command_queue_)))) {
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
    client_w_ = std::max(1u, static_cast<std::uint32_t>(cr.right - cr.left));
    client_h_ = std::max(1u, static_cast<std::uint32_t>(cr.bottom - cr.top));
    viewport_w_ = std::max(4u, std::min(client_w_, 960u));
    viewport_h_ = std::max(4u, std::min(client_h_, 720u));
    if (!CreateViewportTarget(viewport_w_, viewport_h_)) {
        return false;
    }
    if (!CreateDrawPipeline()) {
        return false;
    }

    mesh_vertex_capacity_ = kMaxMeshVertices;
    const D3D12_HEAP_PROPERTIES upload_heap = HeapProps(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC bd = BufferDesc(static_cast<UINT64>(sizeof(GpuVertex)) * static_cast<UINT64>(mesh_vertex_capacity_));
    if (!HrOk(device_->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mesh_vb_)))) {
        return false;
    }
    mesh_vbv_.BufferLocation = mesh_vb_->GetGPUVirtualAddress();
    mesh_vbv_.StrideInBytes = sizeof(GpuVertex);
    mesh_vbv_.SizeInBytes = sizeof(GpuVertex) * mesh_vertex_capacity_;
    return true;
}

bool RigRenderer::CreateSwapchain() {
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

void RigRenderer::CreateBackbuffers() {
    for (UINT i = 0; i < kBackbuffers; ++i) {
        ComPtr<ID3D12Resource> bb;
        if (!HrOk(swapchain_->GetBuffer(i, IID_PPV_ARGS(&bb)))) {
            continue;
        }
        device_->CreateRenderTargetView(bb.Get(), nullptr, backbuffer_rtv_[i]);
        backbuffers_[i] = bb.Detach();
    }
}

void RigRenderer::ReleaseBackbuffers() {
    for (UINT i = 0; i < kBackbuffers; ++i) {
        if (backbuffers_[i]) {
            backbuffers_[i]->Release();
            backbuffers_[i] = nullptr;
        }
    }
}

void RigRenderer::BeginResize() {
    WaitForGpuIdle();
    ReleaseBackbuffers();
}

void RigRenderer::Resize(std::uint32_t w, std::uint32_t h) {
    client_w_ = std::max(1u, w);
    client_h_ = std::max(1u, h);
    if (swapchain_) {
        swapchain_->ResizeBuffers(0, client_w_, client_h_, DXGI_FORMAT_UNKNOWN, 0);
        CreateBackbuffers();
    }
}

void RigRenderer::ResizeViewportTexture(std::uint32_t w, std::uint32_t h) {
    w = std::max(4u, w);
    h = std::max(4u, h);
    if (w == viewport_w_ && h == viewport_h_ && viewport_color_) {
        return;
    }
    viewport_w_ = w;
    viewport_h_ = h;
    WaitForGpuIdle();
    CreateViewportTarget(w, h);
}

bool RigRenderer::CreateViewportTarget(std::uint32_t w, std::uint32_t h) {
    ReleaseViewportTarget();
    const D3D12_HEAP_PROPERTIES default_heap = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    {
        const D3D12_RESOURCE_DESC color_desc = Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, w, h, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        D3D12_CLEAR_VALUE clear{};
        clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clear.Color[0] = 0.08f;
        clear.Color[1] = 0.08f;
        clear.Color[2] = 0.10f;
        clear.Color[3] = 1.f;
        if (!HrOk(device_->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &color_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
                                                   IID_PPV_ARGS(&viewport_color_)))) {
            return false;
        }
        device_->CreateRenderTargetView(viewport_color_, nullptr, viewport_rtv_);
        if (srv_free_.empty()) {
            return false;
        }
        AllocSrvForImGui(&viewport_srv_cpu_, &viewport_srv_gpu_);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        srv.Texture2D.MostDetailedMip = 0;
        device_->CreateShaderResourceView(viewport_color_, &srv, viewport_srv_cpu_);
    }
    {
        const D3D12_RESOURCE_DESC ds_desc = Tex2D(DXGI_FORMAT_D32_FLOAT, w, h, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
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
        device_->CreateDepthStencilView(viewport_depth_, &dsv, viewport_dsv_);
    }
    return true;
}

void RigRenderer::ReleaseViewportTarget() {
    if (viewport_color_) {
        if (viewport_srv_cpu_.ptr) {
            FreeSrvForImGui(viewport_srv_cpu_, viewport_srv_gpu_);
            viewport_srv_cpu_.ptr = 0;
        }
        viewport_color_->Release();
        viewport_color_ = nullptr;
    }
    if (viewport_depth_) {
        viewport_depth_->Release();
        viewport_depth_ = nullptr;
    }
}

bool RigRenderer::CreateDrawPipeline() {
    ComPtr<ID3DBlob> vs, ps, err;
    const UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    if (!HrOk(D3DCompile(kShader, static_cast<UINT>(strlen(kShader)), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compile_flags, 0, &vs, &err))) {
        return false;
    }
    if (!HrOk(D3DCompile(kShader, static_cast<UINT>(strlen(kShader)), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compile_flags, 0, &ps, &err))) {
        return false;
    }
    D3D12_ROOT_PARAMETER rp{};
    rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rp.Constants.ShaderRegister = 0;
    rp.Constants.RegisterSpace = 0;
    rp.Constants.Num32BitValues = 16;
    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters = 1;
    rsd.pParameters = &rp;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> rs;
    if (!HrOk(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &rs, &err))) {
        return false;
    }
    if (!HrOk(device_->CreateRootSignature(0, rs->GetBufferPointer(), rs->GetBufferSize(), IID_PPV_ARGS(&root_sig_)))) {
        return false;
    }
    D3D12_INPUT_ELEMENT_DESC il[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = {il, 2};
    pso.pRootSignature = root_sig_;
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
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

void RigRenderer::WaitForGpuIdle() {
    if (!command_queue_ || !fence_) {
        return;
    }
    const UINT64 v = ++fence_last_;
    command_queue_->Signal(fence_, v);
    if (fence_->GetCompletedValue() < v) {
        fence_->SetEventOnCompletion(v, fence_event_);
        WaitForSingleObject(fence_event_, INFINITE);
    }
}

RigRenderer::FrameContext* RigRenderer::WaitForNextFrame() {
    FrameContext* fc = &frames_[frame_index_ % kFramesInFlight];
    if (fence_->GetCompletedValue() < fc->fence_value) {
        fence_->SetEventOnCompletion(fc->fence_value, fence_event_);
        WaitForSingleObject(fence_event_, INFINITE);
    }
    return fc;
}

void RigRenderer::UploadVertices(const std::vector<GpuVertex>& verts) {
    if (verts.empty() || !mesh_vb_) {
        return;
    }
    if (verts.size() > mesh_vertex_capacity_) {
        return;
    }
    void* mapped = nullptr;
    if (!HrOk(mesh_vb_->Map(0, nullptr, &mapped))) {
        return;
    }
    memcpy(mapped, verts.data(), sizeof(GpuVertex) * verts.size());
    mesh_vb_->Unmap(0, nullptr);
    mesh_vbv_.SizeInBytes = static_cast<UINT>(sizeof(GpuVertex) * verts.size());
}

void RigRenderer::BuildSceneVerts(const RigDocument& doc, std::vector<GpuVertex>& tris, std::vector<GpuVertex>& lines) {
    const XMFLOAT4 grid_col{0.25f, 0.28f, 0.32f, 1.f};
    for (int i = -10; i <= 10; ++i) {
        const float f = static_cast<float>(i) * 0.2f;
        PushLine(lines, XMVectorSet(f, 0.f, -2.f, 0.f), XMVectorSet(f, 0.f, 2.f, 0.f), grid_col);
        PushLine(lines, XMVectorSet(-2.f, 0.f, f, 0.f), XMVectorSet(2.f, 0.f, f, 0.f), grid_col);
    }

    for (std::size_t ti = 0; ti + 2 < doc.mesh.indices.size(); ti += 3) {
        const Vertex& a = doc.skinned_vertices[doc.mesh.indices[ti]];
        const Vertex& b = doc.skinned_vertices[doc.mesh.indices[ti + 1]];
        const Vertex& c = doc.skinned_vertices[doc.mesh.indices[ti + 2]];
        PushTri(tris, XMLoadFloat3(&a.position), XMLoadFloat3(&b.position), XMLoadFloat3(&c.position), a.color);
    }

    const XMFLOAT4 bone_col{0.9f, 0.85f, 0.2f, 1.f};
    for (const Joint& j : doc.skeleton.joints) {
        if (j.parent >= 0) {
            const Joint& p = doc.skeleton.joints[static_cast<std::size_t>(j.parent)];
            XMVECTOR a = XMVector3TransformCoord(XMVectorZero(), XMLoadFloat4x4(&p.world_pose));
            XMVECTOR b = XMVector3TransformCoord(XMVectorZero(), XMLoadFloat4x4(&j.world_pose));
            PushLine(lines, a, b, bone_col);
        }
    }

    for (std::size_t ji = 0; ji < doc.skeleton.joints.size(); ++ji) {
        const Joint& j = doc.skeleton.joints[ji];
        const bool sel = static_cast<int>(ji) == doc.skeleton.selected;
        const XMVECTOR p = XMVector3TransformCoord(XMVectorZero(), XMLoadFloat4x4(&j.world_pose));
        const float s = sel ? 0.06f : 0.035f;
        const XMFLOAT4 jc = JointColor(j, sel);
        PushLine(lines, XMVectorAdd(p, XMVectorSet(-s, 0, 0, 0)), XMVectorAdd(p, XMVectorSet(s, 0, 0, 0)), jc);
        PushLine(lines, XMVectorAdd(p, XMVectorSet(0, -s, 0, 0)), XMVectorAdd(p, XMVectorSet(0, s, 0, 0)), jc);
        PushLine(lines, XMVectorAdd(p, XMVectorSet(0, 0, -s, 0)), XMVectorAdd(p, XMVectorSet(0, 0, s, 0)), jc);
    }

    if (doc.skeleton.selected >= 0 && doc.skeleton.selected < static_cast<int>(doc.skeleton.joints.size())) {
        const Joint& j = doc.skeleton.joints[static_cast<std::size_t>(doc.skeleton.selected)];
        const XMVECTOR o = XMVector3TransformCoord(XMVectorZero(), XMLoadFloat4x4(&j.world_pose));
        const float L = 0.2f;
        PushLine(lines, o, XMVectorAdd(o, XMVectorSet(L, 0, 0, 0)), {1, 0.2f, 0.2f, 1});
        PushLine(lines, o, XMVectorAdd(o, XMVectorSet(0, L, 0, 0)), {0.2f, 1, 0.2f, 1});
        PushLine(lines, o, XMVectorAdd(o, XMVectorSet(0, 0, L, 0)), {0.2f, 0.4f, 1, 1});
    }
}

void RigRenderer::RenderFrame(const RigDocument& doc, const XMMATRIX& view, const XMMATRIX& projection, ImDrawData* imgui_draw_data, bool vsync) {
    FrameContext* fc = WaitForNextFrame();
    const UINT bb = swapchain_->GetCurrentBackBufferIndex();
    fc->command_allocator->Reset();
    command_list_->Reset(fc->command_allocator, nullptr);

    std::vector<GpuVertex> tris, lines;
    tris.reserve(doc.mesh.indices.size());
    lines.reserve(512);
    BuildSceneVerts(doc, tris, lines);
    std::vector<GpuVertex> combined;
    combined.reserve(tris.size() + lines.size());
    combined.insert(combined.end(), tris.begin(), tris.end());
    const std::size_t line_offset = combined.size();
    combined.insert(combined.end(), lines.begin(), lines.end());
    UploadVertices(combined);

    auto to_rtv = Transition(viewport_color_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    command_list_->ResourceBarrier(1, &to_rtv);
    const float cc[4] = {0.08f, 0.08f, 0.10f, 1.f};
    command_list_->ClearRenderTargetView(viewport_rtv_, cc, 0, nullptr);
    command_list_->ClearDepthStencilView(viewport_dsv_, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    D3D12_VIEWPORT vp{0, 0, static_cast<float>(viewport_w_), static_cast<float>(viewport_h_), 0, 1};
    D3D12_RECT sc{0, 0, static_cast<LONG>(viewport_w_), static_cast<LONG>(viewport_h_)};
    command_list_->RSSetViewports(1, &vp);
    command_list_->RSSetScissorRects(1, &sc);
    command_list_->OMSetRenderTargets(1, &viewport_rtv_, FALSE, &viewport_dsv_);
    ID3D12DescriptorHeap* heaps[] = {srv_heap_};
    command_list_->SetDescriptorHeaps(1, heaps);
    command_list_->SetGraphicsRootSignature(root_sig_);
    const XMMATRIX mvp = XMMatrixTranspose(view * projection);
    XMFLOAT4X4 mvp_store{};
    XMStoreFloat4x4(&mvp_store, mvp);
    command_list_->SetGraphicsRoot32BitConstants(0, 16, &mvp_store, 0);
    command_list_->IASetVertexBuffers(0, 1, &mesh_vbv_);
    if (!tris.empty()) {
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->SetPipelineState(pso_tri_);
        mesh_vbv_.SizeInBytes = static_cast<UINT>(sizeof(GpuVertex) * tris.size());
        command_list_->DrawInstanced(static_cast<UINT>(tris.size()), 1, 0, 0);
    }
    if (!lines.empty()) {
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        command_list_->SetPipelineState(pso_line_);
        mesh_vbv_.BufferLocation = mesh_vb_->GetGPUVirtualAddress() + static_cast<UINT64>(line_offset * sizeof(GpuVertex));
        mesh_vbv_.SizeInBytes = static_cast<UINT>(sizeof(GpuVertex) * lines.size());
        command_list_->IASetVertexBuffers(0, 1, &mesh_vbv_);
        command_list_->DrawInstanced(static_cast<UINT>(lines.size()), 1, 0, 0);
        mesh_vbv_.BufferLocation = mesh_vb_->GetGPUVirtualAddress();
    }
    auto to_srv = Transition(viewport_color_, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list_->ResourceBarrier(1, &to_srv);

    auto bb_to_rtv = Transition(backbuffers_[bb], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    command_list_->ResourceBarrier(1, &bb_to_rtv);
    const float ui_cc[4] = {0.12f, 0.12f, 0.12f, 1.f};
    command_list_->ClearRenderTargetView(backbuffer_rtv_[bb], ui_cc, 0, nullptr);
    command_list_->OMSetRenderTargets(1, &backbuffer_rtv_[bb], FALSE, nullptr);
    D3D12_VIEWPORT uvp{0, 0, static_cast<float>(client_w_), static_cast<float>(client_h_), 0, 1};
    D3D12_RECT usc{0, 0, static_cast<LONG>(client_w_), static_cast<LONG>(client_h_)};
    command_list_->RSSetViewports(1, &uvp);
    command_list_->RSSetScissorRects(1, &usc);
    ImGui_ImplDX12_RenderDrawData(imgui_draw_data, command_list_);
    auto bb_to_present = Transition(backbuffers_[bb], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    command_list_->ResourceBarrier(1, &bb_to_present);
    command_list_->Close();
    ID3D12CommandList* lists[] = {command_list_};
    command_queue_->ExecuteCommandLists(1, lists);
    const UINT64 fv = ++fence_last_;
    command_queue_->Signal(fence_, fv);
    fc->fence_value = fv;
    swapchain_->Present(vsync ? 1 : 0, 0);
    frame_index_++;
}

void RigRenderer::Destroy() {
    WaitForGpuIdle();
    if (pso_tri_) {
        pso_tri_->Release();
    }
    if (pso_line_) {
        pso_line_->Release();
    }
    if (root_sig_) {
        root_sig_->Release();
    }
    if (mesh_vb_) {
        mesh_vb_->Release();
    }
    ReleaseViewportTarget();
    BeginResize();
    if (swapchain_) {
        swapchain_->Release();
    }
    for (int i = 0; i < kFramesInFlight; ++i) {
        if (frames_[i].command_allocator) {
            frames_[i].command_allocator->Release();
        }
    }
    if (command_list_) {
        command_list_->Release();
    }
    if (command_queue_) {
        command_queue_->Release();
    }
    if (rtv_heap_) {
        rtv_heap_->Release();
    }
    if (srv_heap_) {
        srv_heap_->Release();
    }
    if (dsv_heap_) {
        dsv_heap_->Release();
    }
    if (fence_) {
        fence_->Release();
    }
    if (fence_event_) {
        CloseHandle(fence_event_);
    }
    if (device_) {
        device_->Release();
    }
}

} // namespace aerigp1
