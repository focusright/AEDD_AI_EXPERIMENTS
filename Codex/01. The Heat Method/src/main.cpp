#include "HeatMethod.h"
#include "d3dx12.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;
using DirectX::XMMATRIX;
using DirectX::XMVECTOR;
using Microsoft::WRL::ComPtr;
using namespace D3DX12;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
constexpr UINT kFrameCount = 2;
constexpr UINT kInitialWidth = 1280;
constexpr UINT kInitialHeight = 800;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
};

struct SceneConstants
{
    XMFLOAT4X4 worldViewProj;
};

void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) {
        std::ostringstream stream;
        stream << "HRESULT failure 0x" << std::hex << static_cast<unsigned int>(hr);
        throw std::runtime_error(stream.str());
    }
}

D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

struct OrbitCamera
{
    float yaw = 0.8f;
    float pitch = 0.3f;
    float distance = 3.0f;

    void UpdateFromImGui()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            const bool rotate = (io.MouseDown[0] && !io.KeyCtrl) || io.MouseDown[1];
            if (rotate) {
                yaw += io.MouseDelta.x * 0.008f;
                pitch += io.MouseDelta.y * 0.008f;
                pitch = std::max(-1.45f, std::min(1.45f, pitch));
            }
            if (std::abs(io.MouseWheel) > 0.0f) {
                distance *= std::pow(0.88f, io.MouseWheel);
                distance = std::max(1.35f, std::min(12.0f, distance));
            }
        }
    }

    XMMATRIX View() const
    {
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const float sy = std::sin(yaw);
        const float cy = std::cos(yaw);
        const XMVECTOR eye = DirectX::XMVectorSet(distance * cp * sy, distance * sp, distance * cp * cy, 1.0f);
        const XMVECTOR target = DirectX::XMVectorZero();
        const XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return DirectX::XMMatrixLookAtLH(eye, target, up);
    }

    XMMATRIX Projection(float aspectRatio) const
    {
        return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), aspectRatio, 0.05f, 100.0f);
    }
};

class HeatMethodApp
{
public:
    void Initialize(HINSTANCE instance);
    int Run();
    void Cleanup();
    void Resize(UINT newWidth, UINT newHeight);

private:
    HWND hwnd = nullptr;
    UINT width = kInitialWidth;
    UINT height = kInitialHeight;
    HeatDemo::HeatMethodSolver solver;
    OrbitCamera camera;
    bool pickSourceWithMouse = true;
    int pendingLongitudeSegments = 48;
    int pendingLatitudeSegments = 24;

    ComPtr<ID3D12Device> device;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;
    UINT frameIndex = 0;

    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    UINT rtvDescriptorSize = 0;

    ComPtr<ID3D12Resource> renderTargets[kFrameCount];
    ComPtr<ID3D12Resource> depthStencil;
    ComPtr<ID3D12Resource> constantBuffer;
    SceneConstants* constants = nullptr;

    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    Vertex* mappedVertices = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
    size_t vertexCapacity = 0;
    size_t indexCapacity = 0;
    UINT indexCount = 0;

    void InitializeWindow(HINSTANCE instance);
    void InitializeD3D12();
    void CreateRenderTargets();
    void CreateDepthBuffer();
    void CreateDescriptorHeaps();
    void CreateConstantBuffer();
    void CreatePipeline();
    void InitializeImGui();
    void RebuildDemo();
    void UpdateMeshBuffers(bool forceRecreate);
    void RenderFrame();
    void DrawUi();
    void TryPickSource();
    void ReportStatus() const;
    void WaitForGpu();
};

HeatMethodApp* g_app = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
        return true;
    }

    switch (message) {
    case WM_SIZE:
        if (g_app && wParam != SIZE_MINIMIZED) {
            const UINT newWidth = static_cast<UINT>(LOWORD(lParam));
            const UINT newHeight = static_cast<UINT>(HIWORD(lParam));
            if (newWidth > 0 && newHeight > 0) {
                g_app->Resize(newWidth, newHeight);
            }
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

void HeatMethodApp::Initialize(HINSTANCE instance)
{
    InitializeWindow(instance);
    InitializeD3D12();
    CreateDescriptorHeaps();
    CreateRenderTargets();
    CreateDepthBuffer();
    CreateConstantBuffer();
    CreatePipeline();
    InitializeImGui();
    RebuildDemo();
}

int HeatMethodApp::Run()
{
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            RenderFrame();
        }
    }
    return static_cast<int>(msg.wParam);
}

void HeatMethodApp::InitializeWindow(HINSTANCE instance)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"HeatMethodDx12DemoWindow";
    RegisterClassEx(&wc);

    RECT rect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    const DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&rect, style, FALSE);

    hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"Heat Method DX12 + ImGui Demo",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd) {
        throw std::runtime_error("CreateWindowEx failed");
    }

    ShowWindow(hwnd, SW_SHOW);
}

void HeatMethodApp::InitializeD3D12()
{
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIFactory6> factory;
    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
            break;
        }
    }

    if (!device) {
        ComPtr<IDXGIAdapter> warp;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.BufferCount = kFrameCount;
    swapDesc.Width = width;
    swapDesc.Height = height;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapDesc, nullptr, nullptr, &swapChain1));
    ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain1.As(&swapChain));
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
    ThrowIfFailed(commandList->Close());

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void HeatMethodApp::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = kFrameCount;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap)));
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&dsvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = 1;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&srvHeap)));
}

void HeatMethodApp::CreateRenderTargets()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i) {
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, handle);
        handle.ptr += rtvDescriptorSize;
    }
}

void HeatMethodApp::CreateDepthBuffer()
{
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencil)));

    D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
    viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depthStencil.Get(), &viewDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void HeatMethodApp::Resize(UINT newWidth, UINT newHeight)
{
    if (newWidth == 0 || newHeight == 0 || (newWidth == width && newHeight == height)) {
        return;
    }

    width = newWidth;
    height = newHeight;

    if (!device || !swapChain || !rtvHeap || !dsvHeap) {
        return;
    }

    WaitForGpu();
    for (ComPtr<ID3D12Resource>& renderTarget : renderTargets) {
        renderTarget.Reset();
    }
    depthStencil.Reset();

    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    ThrowIfFailed(swapChain->GetDesc(&swapDesc));
    ThrowIfFailed(swapChain->ResizeBuffers(kFrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, swapDesc.Flags));
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    CreateRenderTargets();
    CreateDepthBuffer();
}

void HeatMethodApp::CreateConstantBuffer()
{
    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneConstants) + 255u) & ~255u);
    const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)));

    D3D12_RANGE readRange = {0, 0};
    void* mapped = nullptr;
    ThrowIfFailed(constantBuffer->Map(0, &readRange, &mapped));
    constants = static_cast<SceneConstants*>(mapped);
    std::memset(constants, 0, sizeof(SceneConstants));
}

void HeatMethodApp::CreatePipeline()
{
    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.Descriptor.ShaderRegister = 0;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &rootParameter;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob));
    ThrowIfFailed(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

    const char* shaderSource = R"(
cbuffer SceneConstants : register(b0)
{
    float4x4 worldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.normal = input.normal;
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(float3(0.35f, 0.70f, -0.45f));
    float lighting = 0.35f + 0.65f * saturate(dot(normal, lightDir));
    return float4(input.color.rgb * lighting, input.color.a);
}
)";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ThrowIfFailed(D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob));
    ThrowIfFailed(D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob));

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex, position)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex, normal)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex, color)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.InputLayout = {inputLayout, _countof(inputLayout)};
    psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlend = {};
    renderTargetBlend.BlendEnable = FALSE;
    renderTargetBlend.LogicOpEnable = FALSE;
    renderTargetBlend.SrcBlend = D3D12_BLEND_ONE;
    renderTargetBlend.DestBlend = D3D12_BLEND_ZERO;
    renderTargetBlend.BlendOp = D3D12_BLEND_OP_ADD;
    renderTargetBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTargetBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTargetBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
    renderTargetBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[0] = renderTargetBlend;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
}

void HeatMethodApp::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = device.Get();
    initInfo.CommandQueue = commandQueue.Get();
    initInfo.NumFramesInFlight = kFrameCount;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    initInfo.SrvDescriptorHeap = srvHeap.Get();
    initInfo.LegacySingleSrvCpuDescriptor = srvHeap->GetCPUDescriptorHandleForHeapStart();
    initInfo.LegacySingleSrvGpuDescriptor = srvHeap->GetGPUDescriptorHandleForHeapStart();
    if (!ImGui_ImplDX12_Init(&initInfo)) {
        throw std::runtime_error("ImGui_ImplDX12_Init failed");
    }
}

void HeatMethodApp::RebuildDemo()
{
    solver.longitudeSegments = pendingLongitudeSegments;
    solver.latitudeSegments = pendingLatitudeSegments;
    solver.RebuildMeshAndFactors();
    pendingLongitudeSegments = solver.longitudeSegments;
    pendingLatitudeSegments = solver.latitudeSegments;
    UpdateMeshBuffers(true);
    ReportStatus();
}

void HeatMethodApp::UpdateMeshBuffers(bool forceRecreate)
{
    const auto& positions = solver.DisplayPositions();
    const auto& normals = solver.Normals();
    const auto& colors = solver.Colors();
    const auto& indices = solver.Indices();
    if (positions.empty() || indices.empty()) {
        return;
    }

    const size_t vertexCount = positions.size();
    const size_t newIndexCount = indices.size();
    if (forceRecreate || vertexCount > vertexCapacity) {
        WaitForGpu();
        if (vertexBuffer && mappedVertices) {
            vertexBuffer->Unmap(0, nullptr);
            mappedVertices = nullptr;
        }
        vertexCapacity = vertexCount;
        const auto desc = CD3DX12_RESOURCE_DESC::Buffer(vertexCapacity * sizeof(Vertex));
        const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer)));

        D3D12_RANGE readRange = {0, 0};
        void* mapped = nullptr;
        ThrowIfFailed(vertexBuffer->Map(0, &readRange, &mapped));
        mappedVertices = static_cast<Vertex*>(mapped);
        vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexBufferView.SizeInBytes = static_cast<UINT>(vertexCapacity * sizeof(Vertex));
        vertexBufferView.StrideInBytes = sizeof(Vertex);
    }

    for (size_t i = 0; i < vertexCount; ++i) {
        mappedVertices[i] = {
            XMFLOAT3(static_cast<float>(positions[i].x), static_cast<float>(positions[i].y), static_cast<float>(positions[i].z)),
            XMFLOAT3(static_cast<float>(normals[i].x), static_cast<float>(normals[i].y), static_cast<float>(normals[i].z)),
            XMFLOAT4(colors[i].r, colors[i].g, colors[i].b, colors[i].a)
        };
    }

    if (forceRecreate || newIndexCount > indexCapacity) {
        WaitForGpu();
        indexCapacity = newIndexCount;
        const auto desc = CD3DX12_RESOURCE_DESC::Buffer(indexCapacity * sizeof(uint32_t));
        const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexBuffer)));
        indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        indexBufferView.SizeInBytes = static_cast<UINT>(indexCapacity * sizeof(uint32_t));
        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    }

    void* mappedIndexData = nullptr;
    D3D12_RANGE readRange = {0, 0};
    ThrowIfFailed(indexBuffer->Map(0, &readRange, &mappedIndexData));
    std::memcpy(mappedIndexData, indices.data(), newIndexCount * sizeof(uint32_t));
    indexBuffer->Unmap(0, nullptr);
    indexCount = static_cast<UINT>(newIndexCount);
}

void HeatMethodApp::RenderFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    camera.UpdateFromImGui();
    TryPickSource();
    DrawUi();

    ImGui::Render();

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const XMMATRIX mvp = camera.View() * camera.Projection(aspect);
    DirectX::XMStoreFloat4x4(&constants->worldViewProj, DirectX::XMMatrixTranspose(mvp));

    ThrowIfFailed(commandAllocator->Reset());
    ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    const auto toRenderTarget = TransitionBarrier(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = {0.03f, 0.035f, 0.045f, 1.0f};
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
    if (indexCount > 0) {
        commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {srvHeap.Get()};
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

    const auto toPresent = TransitionBarrier(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &toPresent);
    ThrowIfFailed(commandList->Close());

    ID3D12CommandList* lists[] = {commandList.Get()};
    commandQueue->ExecuteCommandLists(1, lists);
    ThrowIfFailed(swapChain->Present(1, 0));
    WaitForGpu();
    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void HeatMethodApp::DrawUi()
{
    ImGui::SetNextWindowPos(ImVec2(14.0f, 14.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(365.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Heat Method Demo");

    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* meshNames[] = {
            "Sphere",
            "U-Folded Plane",
            "Swiss Roll",
            "Bunny / Suzanne"
        };
        int meshKind = static_cast<int>(solver.meshKind);
        if (ImGui::Combo("Mesh Type", &meshKind, meshNames, IM_ARRAYSIZE(meshNames))) {
            solver.meshKind = static_cast<HeatDemo::MeshKind>(meshKind);
            RebuildDemo();
        }
        ImGui::SliderInt("U/Longitude Segments", &pendingLongitudeSegments, 12, 96);
        ImGui::SliderInt("V/Latitude Segments", &pendingLatitudeSegments, 6, 48);
        if (ImGui::Button("Rebuild Mesh")) {
            RebuildDemo();
        }
    }

    if (ImGui::CollapsingHeader("Source", ImGuiTreeNodeFlags_DefaultOpen)) {
        int source = solver.sourceVertex;
        const int maxSource = std::max(0, solver.Diagnostics().vertexCount - 1);
        if (ImGui::SliderInt("Source Vertex", &source, 0, maxSource)) {
            source = std::max(0, std::min(source, maxSource));
            solver.sourceVertex = source;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                solver.RecomputeDistance();
                UpdateMeshBuffers(false);
                ReportStatus();
            }
        }
        ImGui::Checkbox("Pick Source With Mouse", &pickSourceWithMouse);
        if (ImGui::Button("Recompute Distance")) {
            solver.sourceVertex = std::max(0, std::min(solver.sourceVertex, maxSource));
            solver.RecomputeDistance();
            UpdateMeshBuffers(false);
            ReportStatus();
        }
    }

    if (ImGui::CollapsingHeader("Heat Method", ImGuiTreeNodeFlags_DefaultOpen)) {
        float scale = static_cast<float>(solver.timestepScale);
        if (ImGui::SliderFloat("Time Step Scale m", &scale, 0.05f, 5.0f, "%.3f")) {
            solver.timestepScale = static_cast<double>(scale);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                solver.RefactorHeatMatrix();
                UpdateMeshBuffers(false);
                ReportStatus();
            }
        }
        ImGui::Text("Actual timestep t = %.6f", solver.Diagnostics().timestep);
        if (ImGui::Button("Refactor Matrices")) {
            solver.RefactorMatrices();
            UpdateMeshBuffers(false);
            ReportStatus();
        }
        if (ImGui::Button("Recompute Using Existing Factors")) {
            solver.RecomputeDistance();
            UpdateMeshBuffers(false);
            ReportStatus();
        }
    }

    if (ImGui::CollapsingHeader("Soft Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool visualChanged = false;
        float radius = static_cast<float>(solver.softRadius);
        float falloff = static_cast<float>(solver.falloffPower);
        float displacement = static_cast<float>(solver.displacementAmount);
        if (ImGui::SliderFloat("Radius", &radius, 0.05f, 3.2f, "%.3f")) {
            solver.softRadius = static_cast<double>(radius);
            visualChanged = true;
        }
        if (ImGui::SliderFloat("Falloff Power", &falloff, 0.1f, 8.0f, "%.3f")) {
            solver.falloffPower = static_cast<double>(falloff);
            visualChanged = true;
        }
        if (ImGui::Checkbox("Use Smoothstep", &solver.useSmoothstep)) {
            visualChanged = true;
        }
        if (ImGui::SliderFloat("Displacement Amount", &displacement, 0.0f, 0.5f, "%.3f")) {
            solver.displacementAmount = static_cast<double>(displacement);
            visualChanged = true;
        }
        if (visualChanged) {
            solver.RecomputeWeightsAndVisuals();
            UpdateMeshBuffers(false);
        }
    }

    if (ImGui::CollapsingHeader("Visualization Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* names[] = {
            "Distance",
            "Distance Bands",
            "Soft Weight",
            "Displaced Soft Weight",
            "Analytic Error"
        };
        int mode = static_cast<int>(solver.visualizationMode);
        if (ImGui::Combo("Mode", &mode, names, IM_ARRAYSIZE(names))) {
            solver.visualizationMode = static_cast<HeatDemo::VisualizationMode>(mode);
            solver.RecomputeWeightsAndVisuals();
            UpdateMeshBuffers(false);
        }
    }

    if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
        const HeatDemo::HeatDiagnostics& d = solver.Diagnostics();
        ImGui::Text("Mesh: %s", d.meshName.c_str());
        ImGui::Text("Vertices: %d", d.vertexCount);
        ImGui::Text("Triangles: %d", d.triangleCount);
        ImGui::Text("Analytic reference: %s", d.analyticErrorAvailable ? "sphere" : "not available");
        ImGui::Text("Mean edge h: %.6f", d.meanEdgeLength);
        ImGui::Text("Timestep t: %.6f", d.timestep);
        ImGui::Text("Degenerate triangles: %d", d.degenerateTriangleCount);
        ImGui::Text("Heat factor: %s%s", d.heatFactorOk ? "ok" : "failed", d.heatRegularized ? " (regularized)" : "");
        ImGui::Text("Poisson factor: %s%s", d.poissonFactorOk ? "ok" : "failed", d.poissonRegularized ? " (regularized)" : "");
        ImGui::Text("K symmetry max: %.3e", d.stiffnessSymmetryError);
        ImGui::Text("K row sum max: %.3e", d.stiffnessRowSumMax);
        ImGui::Text("u min/max: %.6g / %.6g", d.minHeat, d.maxHeat);
        ImGui::Text("phi min/max: %.6g / %.6g", d.minPhi, d.maxPhi);
        ImGui::Text("weight min/max: %.6g / %.6g", d.minWeight, d.maxWeight);
        ImGui::Text("mean abs error: %.6g", d.meanAbsError);
        ImGui::Text("max abs error: %.6g", d.maxAbsError);
        ImGui::Text("mean/max relative: %.6g / %.6g", d.meanRelativeError, d.maxRelativeError);
        ImGui::Separator();
        ImGui::TextWrapped("%s", d.status.c_str());
    }

    ImGui::End();
}

void HeatMethodApp::TryPickSource()
{
    ImGuiIO& io = ImGui::GetIO();
    if (!pickSourceWithMouse || io.WantCaptureMouse || !io.KeyCtrl || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    const auto& positions = solver.BasePositions();
    if (positions.empty()) {
        return;
    }

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const XMMATRIX mvp = camera.View() * camera.Projection(aspect);
    const float mouseX = io.MousePos.x;
    const float mouseY = io.MousePos.y;

    int bestVertex = -1;
    float bestDistanceSq = 15.0f * 15.0f;
    for (size_t i = 0; i < positions.size(); ++i) {
        const HeatDemo::Vec3& p = positions[i];
        XMVECTOR world = DirectX::XMVectorSet(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z), 1.0f);
        XMVECTOR clip = DirectX::XMVector4Transform(world, mvp);
        const float w = DirectX::XMVectorGetW(clip);
        if (w <= 1e-5f) {
            continue;
        }

        const float ndcX = DirectX::XMVectorGetX(clip) / w;
        const float ndcY = DirectX::XMVectorGetY(clip) / w;
        const float ndcZ = DirectX::XMVectorGetZ(clip) / w;
        if (ndcZ < 0.0f || ndcZ > 1.0f || std::abs(ndcX) > 1.2f || std::abs(ndcY) > 1.2f) {
            continue;
        }

        const float sx = (ndcX * 0.5f + 0.5f) * static_cast<float>(width);
        const float sy = (-ndcY * 0.5f + 0.5f) * static_cast<float>(height);
        const float dx = sx - mouseX;
        const float dy = sy - mouseY;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestVertex = static_cast<int>(i);
        }
    }

    if (bestVertex >= 0) {
        solver.sourceVertex = bestVertex;
        solver.RecomputeDistance();
        UpdateMeshBuffers(false);
        ReportStatus();
    }
}

void HeatMethodApp::ReportStatus() const
{
    const std::string text = solver.Diagnostics().status + "\n";
    OutputDebugStringA(text.c_str());
}

void HeatMethodApp::WaitForGpu()
{
    if (!commandQueue || !fence) {
        return;
    }

    const UINT64 value = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(fence.Get(), value));
    if (fence->GetCompletedValue() < value) {
        ThrowIfFailed(fence->SetEventOnCompletion(value, fenceEvent));
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

void HeatMethodApp::Cleanup()
{
    if (commandQueue && fence) {
        WaitForGpu();
    }

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (vertexBuffer && mappedVertices) {
        vertexBuffer->Unmap(0, nullptr);
        mappedVertices = nullptr;
    }
    if (constantBuffer && constants) {
        constantBuffer->Unmap(0, nullptr);
        constants = nullptr;
    }
    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    HeatMethodApp app;
    g_app = &app;
    try {
        app.Initialize(instance);
        const int result = app.Run();
        app.Cleanup();
        return result;
    } catch (const std::exception& e) {
        app.Cleanup();
        MessageBoxA(nullptr, e.what(), "Heat Method DX12 Demo", MB_OK | MB_ICONERROR);
        return -1;
    }
}
