#include "d3d_presenter.h"
#include "d3d_draw_model.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"DOAXBVRecompPresenterWindow";
constexpr wchar_t kWindowTitle[] = L"DOAXBV Recomp";

/* Untransformed positions with a host-side world-view-projection composite.

   Vertex components other than position vary by FVF: this title draws both
   XYZ|NORMAL|TEX1 and XYZ|DIFFUSE|TEX1, whose texcoords sit at different
   offsets and whose colors come from different sources. One shader cannot
   serve both, so the shader is assembled per FVF from the decoded layout and
   cached with its matching input layout.

   Vertex color is the guest's when the FVF carries one, otherwise the normal
   visualized as |n|. A bound texture modulates that color; `textured` is a
   shader constant rather than a second pipeline variant because the same FVF
   is drawn both with and without a texture. Real lighting is a separate
   seam. */
constexpr char kDrawShaderPrologue[] =
    "cbuffer Transform : register(b0) {\n"
    "    row_major float4x4 wvp;\n"
    "    float4 draw_flags;\n"
    "}\n"
    "Texture2D guest_texture : register(t0);\n"
    "SamplerState guest_sampler : register(s0);\n";

/* Builds the draw shader for one decoded vertex layout. Only the components
   the layout actually carries appear in VSIn, so the input layout and the
   shader signature always agree. */
void buildDrawShaderSource(
    const RecompD3dVertexLayout &layout,
    char *out,
    size_t out_size)
{
    const bool has_normal =
        layout.normal_offset != RECOMP_D3D_FVF_ABSENT;
    const bool has_diffuse =
        layout.diffuse_offset != RECOMP_D3D_FVF_ABSENT;
    const bool has_texcoord =
        layout.texcoord_offset != RECOMP_D3D_FVF_ABSENT;

    std::snprintf(
        out,
        out_size,
        "%s"
        "struct VSIn {\n"
        "    float3 position : POSITION;\n"
        "%s%s%s"
        "};\n"
        "struct VSOut {\n"
        "    float4 position : SV_POSITION;\n"
        "    float4 color : COLOR0;\n"
        "    float2 texcoord : TEXCOORD0;\n"
        "};\n"
        "VSOut vs_main(VSIn input) {\n"
        "    VSOut output;\n"
        "    output.position = mul(float4(input.position, 1.0f), wvp);\n"
        "%s"
        "%s"
        "    return output;\n"
        "}\n"
        "float4 ps_main(VSOut input) : SV_TARGET {\n"
        "%s"
        "}\n",
        kDrawShaderPrologue,
        has_normal ? "    float3 normal : NORMAL;\n" : "",
        has_diffuse ? "    float4 diffuse : COLOR0;\n" : "",
        has_texcoord ? "    float2 texcoord : TEXCOORD0;\n" : "",
        /* Guest vertex color when the stream has one; otherwise the normal
           stands in so surfaces remain distinguishable. */
        has_diffuse
            ? "    output.color = input.diffuse;\n"
            : (has_normal
                   ? "    output.color = "
                     "float4(abs(normalize(input.normal)), 1.0f);\n"
                   : "    output.color = float4(0.75f, 0.75f, 0.78f, 1.0f);\n"),
        has_texcoord
            ? "    output.texcoord = input.texcoord;\n"
            : "    output.texcoord = float2(0.0f, 0.0f);\n",
        /* Blending is live, so a hardcoded alpha of 1.0 would saturate every
           SRC_ALPHA-weighted blend. Pass guest alpha through when the stream
           carries one. */
        has_diffuse
            ? "    float4 shaded = input.color;\n"
              "    if (draw_flags.x > 0.5f) {\n"
              "        shaded *= guest_texture.Sample("
              "guest_sampler, input.texcoord);\n"
              "    }\n"
              "    return shaded;\n"
            /* Without a diffuse component the fixed-function default is
               white, so a textured draw must show the texture itself. The
               normal stand-in only applies when nothing is bound. */
            : "    if (draw_flags.x > 0.5f) {\n"
              "        return float4(guest_texture.Sample("
              "guest_sampler, input.texcoord).rgb, 1.0f);\n"
              "    }\n"
              "    return float4(input.color.rgb, 1.0f);\n");
}

LRESULT CALLBACK presenterWindowProc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

template <typename T>
void releaseCom(T *&object)
{
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

/* Compiled state for one FVF. */
constexpr uint32_t kDrawPipelineSlots = 8u;

struct DrawPipeline {
    uint32_t fvf;
    bool used;
    bool failed;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
};

/* Depth-stencil states are few and repeat every frame, so they are cached by
   the guest state that produced them rather than rebuilt per draw. */
constexpr uint32_t kDepthStateSlots = 16u;

struct DepthStateEntry {
    bool used;
    bool test_enable;
    bool write_enable;
    RecompD3dCompareFunc func;
    ID3D11DepthStencilState *state;
};

constexpr uint32_t kBlendStateSlots = 16u;

struct BlendStateEntry {
    bool used;
    bool enable;
    RecompD3dBlendFactor src;
    RecompD3dBlendFactor dst;
    RecompD3dBlendOp op;
    ID3D11BlendState *state;
};

/* Guest textures are uploaded once and then reused by guest address. The
   guest reuses an address after freeing it, so the decoded shape is part of
   the key. */
constexpr uint32_t kTextureSlots = 256u;

struct TextureEntry {
    bool used;
    uint32_t data;
    uint32_t format_byte;
    uint32_t width;
    uint32_t height;
    ID3D11ShaderResourceView *view;
};

} // namespace

struct RecompD3dPresenter {
    RecompD3dPresenterConfig config{};
    DWORD owner_thread = 0;
    HINSTANCE instance = nullptr;
    bool owns_window_class = false;
    HWND window = nullptr;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    IDXGISwapChain *swap_chain = nullptr;
    ID3D11RenderTargetView *render_target_view = nullptr;
    ID3D11Texture2D *depth_texture = nullptr;
    ID3D11DepthStencilView *depth_view = nullptr;
    ID3D11Buffer *draw_vertex_buffer = nullptr;
    ID3D11Buffer *draw_index_buffer = nullptr;
    ID3D11Buffer *draw_constant_buffer = nullptr;
    ID3D11RasterizerState *draw_rasterizer_state = nullptr;
    UINT draw_vertex_capacity = 0u;
    UINT draw_index_capacity = 0u;
    /* One pipeline per FVF. A failure is recorded against its own FVF so a
       stream this seam cannot build never disables the ones it can. */
    DrawPipeline draw_pipelines[kDrawPipelineSlots]{};
    uint32_t draw_pipeline_count = 0u;
    DepthStateEntry depth_states[kDepthStateSlots]{};
    uint32_t depth_state_count = 0u;
    BlendStateEntry blend_states[kBlendStateSlots]{};
    uint32_t blend_state_count = 0u;
    TextureEntry textures[kTextureSlots]{};
    uint32_t texture_count = 0u;
    ID3D11SamplerState *draw_sampler = nullptr;
    bool draw_shared_ready = false;
    bool draw_shared_failed = false;
    uint32_t draw_count = 0u;
    bool first_draw_reported = false;
    const char *driver_name = "unknown";
    HRESULT create_result = E_FAIL;
    uint32_t present_count = 0;
    bool first_present_reported = false;
};

namespace {

RecompD3dPresenter *active_presenter;

static bool immediate_present;

bool configSupported(const RecompD3dPresenterConfig &config)
{
    return config.width != 0u && config.height != 0u &&
        config.color_format ==
            RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM &&
        config.depth_format == RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8;
}

void releaseGraphics(RecompD3dPresenter *presenter)
{
    if (presenter->context != nullptr) {
        presenter->context->OMSetRenderTargets(0u, nullptr, nullptr);
        presenter->context->ClearState();
        presenter->context->Flush();
    }
    releaseCom(presenter->draw_rasterizer_state);
    releaseCom(presenter->draw_constant_buffer);
    releaseCom(presenter->draw_index_buffer);
    releaseCom(presenter->draw_vertex_buffer);
    for (uint32_t i = 0u; i < presenter->draw_pipeline_count; ++i) {
        DrawPipeline &pipeline = presenter->draw_pipelines[i];

        releaseCom(pipeline.input_layout);
        releaseCom(pipeline.pixel_shader);
        releaseCom(pipeline.vertex_shader);
        pipeline.used = false;
    }
    presenter->draw_pipeline_count = 0u;
    for (uint32_t i = 0u; i < presenter->depth_state_count; ++i) {
        DepthStateEntry &entry = presenter->depth_states[i];

        releaseCom(entry.state);
        entry.used = false;
    }
    presenter->depth_state_count = 0u;
    for (uint32_t i = 0u; i < presenter->blend_state_count; ++i) {
        BlendStateEntry &entry = presenter->blend_states[i];

        releaseCom(entry.state);
        entry.used = false;
    }
    presenter->blend_state_count = 0u;
    for (uint32_t i = 0u; i < presenter->texture_count; ++i) {
        TextureEntry &entry = presenter->textures[i];

        releaseCom(entry.view);
        entry.used = false;
    }
    presenter->texture_count = 0u;
    releaseCom(presenter->draw_sampler);
    presenter->draw_vertex_capacity = 0u;
    presenter->draw_index_capacity = 0u;
    presenter->draw_shared_ready = false;
    releaseCom(presenter->depth_view);
    releaseCom(presenter->depth_texture);
    releaseCom(presenter->render_target_view);
    releaseCom(presenter->swap_chain);
    releaseCom(presenter->context);
    releaseCom(presenter->device);
}

void releasePresenter(RecompD3dPresenter *presenter)
{
    releaseGraphics(presenter);
    if (presenter->window != nullptr && IsWindow(presenter->window)) {
        DestroyWindow(presenter->window);
    }
    presenter->window = nullptr;
    if (presenter->owns_window_class) {
        UnregisterClassW(kWindowClassName, presenter->instance);
        presenter->owns_window_class = false;
    }
}

bool createWindow(RecompD3dPresenter *presenter)
{
    WNDCLASSEXW window_class{};
    RECT window_rect = {
        0,
        0,
        static_cast<LONG>(presenter->config.width),
        static_cast<LONG>(presenter->config.height),
    };
    constexpr DWORD style = WS_OVERLAPPEDWINDOW;

    presenter->instance = GetModuleHandleW(nullptr);
    window_class.cbSize = sizeof window_class;
    window_class.lpfnWndProc = presenterWindowProc;
    window_class.hInstance = presenter->instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;

    const ATOM window_class_atom = RegisterClassExW(&window_class);
    if (window_class_atom == 0u) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    } else {
        presenter->owns_window_class = true;
    }
    if (!AdjustWindowRectEx(&window_rect, style, FALSE, 0u)) {
        return false;
    }

    presenter->window = CreateWindowExW(
        0u,
        kWindowClassName,
        kWindowTitle,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr,
        nullptr,
        presenter->instance,
        nullptr);
    if (presenter->window == nullptr) {
        return false;
    }

    RECT client_rect{};
    return GetClientRect(presenter->window, &client_rect) &&
        client_rect.right - client_rect.left ==
            static_cast<LONG>(presenter->config.width) &&
        client_rect.bottom - client_rect.top ==
            static_cast<LONG>(presenter->config.height);
}

HRESULT createDeviceWithDriver(
    RecompD3dPresenter *presenter,
    D3D_DRIVER_TYPE driver_type)
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selected_feature_level{};

    swap_chain_desc.BufferDesc.Width = presenter->config.width;
    swap_chain_desc.BufferDesc.Height = presenter->config.height;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 0u;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1u;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.SampleDesc.Count = 1u;
    swap_chain_desc.SampleDesc.Quality = 0u;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 1u;
    swap_chain_desc.OutputWindow = presenter->window;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    return D3D11CreateDeviceAndSwapChain(
        nullptr,
        driver_type,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels,
        static_cast<UINT>(sizeof feature_levels / sizeof feature_levels[0]),
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &presenter->swap_chain,
        &presenter->device,
        &selected_feature_level,
        &presenter->context);
}

HRESULT createGraphics(RecompD3dPresenter *presenter)
{
    HRESULT hardware_result = createDeviceWithDriver(
        presenter, D3D_DRIVER_TYPE_HARDWARE);

    if (SUCCEEDED(hardware_result)) {
        presenter->driver_name = "hardware";
        presenter->create_result = hardware_result;
    } else {
        std::fprintf(
            stderr,
            "recomp d3d presenter: hardware create failed hr=0x%08lX; "
            "trying WARP\n",
            static_cast<unsigned long>(hardware_result));
        releaseGraphics(presenter);
        const HRESULT warp_result = createDeviceWithDriver(
            presenter, D3D_DRIVER_TYPE_WARP);
        if (FAILED(warp_result)) {
            std::fprintf(
                stderr,
                "recomp d3d presenter: WARP create failed hr=0x%08lX\n",
                static_cast<unsigned long>(warp_result));
            return warp_result;
        }
        presenter->driver_name = "warp";
        presenter->create_result = warp_result;
        std::fprintf(stderr, "recomp d3d presenter: using WARP driver\n");
    }

    ID3D11Texture2D *back_buffer = nullptr;
    HRESULT result = presenter->swap_chain->GetBuffer(
        0u,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void **>(&back_buffer));
    if (SUCCEEDED(result)) {
        result = presenter->device->CreateRenderTargetView(
            back_buffer, nullptr, &presenter->render_target_view);
    }
    releaseCom(back_buffer);
    if (FAILED(result)) {
        return result;
    }

    D3D11_TEXTURE2D_DESC depth_desc{};
    depth_desc.Width = presenter->config.width;
    depth_desc.Height = presenter->config.height;
    depth_desc.MipLevels = 1u;
    depth_desc.ArraySize = 1u;
    depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.SampleDesc.Count = 1u;
    depth_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    result = presenter->device->CreateTexture2D(
        &depth_desc, nullptr, &presenter->depth_texture);
    if (FAILED(result)) {
        return result;
    }
    result = presenter->device->CreateDepthStencilView(
        presenter->depth_texture, nullptr, &presenter->depth_view);
    if (FAILED(result)) {
        return result;
    }

    presenter->context->OMSetRenderTargets(
        1u, &presenter->render_target_view, presenter->depth_view);
    const D3D11_VIEWPORT viewport = {
        0.0f,
        0.0f,
        static_cast<float>(presenter->config.width),
        static_cast<float>(presenter->config.height),
        0.0f,
        1.0f,
    };
    presenter->context->RSSetViewports(1u, &viewport);
    return S_OK;
}

RecompD3dPresenterError submitClear(
    RecompD3dPresenter *presenter,
    const RecompD3dPresenterClearCommand &clear)
{
    if (!clear.clear_color || !clear.clear_depth ||
        !clear.clear_stencil || !std::isfinite(clear.z) ||
        clear.z < 0.0f || clear.z > 1.0f || clear.stencil > 0xffu) {
        return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    }

    constexpr float byte_to_float = 1.0f / 255.0f;
    const float color[] = {
        static_cast<float>((clear.color >> 16u) & 0xffu) * byte_to_float,
        static_cast<float>((clear.color >> 8u) & 0xffu) * byte_to_float,
        static_cast<float>(clear.color & 0xffu) * byte_to_float,
        static_cast<float>((clear.color >> 24u) & 0xffu) * byte_to_float,
    };
    presenter->context->ClearRenderTargetView(
        presenter->render_target_view, color);
    presenter->context->ClearDepthStencilView(
        presenter->depth_view,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        clear.z,
        static_cast<UINT8>(clear.stencil));
    return FAILED(presenter->device->GetDeviceRemovedReason())
        ? RECOMP_D3D_PRESENTER_HOST_FAILURE
        : RECOMP_D3D_PRESENTER_OK;
}

bool compileDrawShader(
    const char *source,
    const char *entry_point,
    const char *target,
    ID3DBlob **blob)
{
    ID3DBlob *errors = nullptr;
    const HRESULT result = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entry_point,
        target,
        0u,
        0u,
        blob,
        &errors);

    if (FAILED(result)) {
        std::fprintf(
            stderr,
            "recomp d3d presenter: draw shader %s failed hr=0x%08lX %s\n",
            entry_point,
            static_cast<unsigned long>(result),
            errors != nullptr
                ? static_cast<const char *>(errors->GetBufferPointer())
                : "");
    }
    releaseCom(errors);
    return SUCCEEDED(result);
}

/* Builds the shader pair and input layout for one FVF from its decoded
   component offsets. */
bool createDrawPipeline(
    RecompD3dPresenter *presenter,
    uint32_t fvf,
    DrawPipeline &pipeline)
{
    RecompD3dVertexLayout layout;

    if (!recomp_d3d_fvf_layout(fvf, &layout)) {
        return false;
    }

    char source[2048];
    buildDrawShaderSource(layout, source, sizeof source);

    ID3DBlob *vertex_blob = nullptr;
    ID3DBlob *pixel_blob = nullptr;
    if (!compileDrawShader(source, "vs_main", "vs_4_0", &vertex_blob) ||
        !compileDrawShader(source, "ps_main", "ps_4_0", &pixel_blob)) {
        releaseCom(vertex_blob);
        releaseCom(pixel_blob);
        return false;
    }

    HRESULT result = presenter->device->CreateVertexShader(
        vertex_blob->GetBufferPointer(),
        vertex_blob->GetBufferSize(),
        nullptr,
        &pipeline.vertex_shader);
    if (SUCCEEDED(result)) {
        result = presenter->device->CreatePixelShader(
            pixel_blob->GetBufferPointer(),
            pixel_blob->GetBufferSize(),
            nullptr,
            &pipeline.pixel_shader);
    }
    if (SUCCEEDED(result)) {
        D3D11_INPUT_ELEMENT_DESC elements[4]{};
        UINT count = 0u;

        elements[count++] = {
            "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u,
            layout.position_offset, D3D11_INPUT_PER_VERTEX_DATA, 0u};
        if (layout.normal_offset != RECOMP_D3D_FVF_ABSENT) {
            elements[count++] = {
                "NORMAL", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u,
                layout.normal_offset, D3D11_INPUT_PER_VERTEX_DATA, 0u};
        }
        if (layout.diffuse_offset != RECOMP_D3D_FVF_ABSENT) {
            /* Xbox diffuse is packed A8R8G8B8, so the BGRA host format
               delivers the channels in the order the shader expects. */
            elements[count++] = {
                "COLOR", 0u, DXGI_FORMAT_B8G8R8A8_UNORM, 0u,
                layout.diffuse_offset, D3D11_INPUT_PER_VERTEX_DATA, 0u};
        }
        if (layout.texcoord_offset != RECOMP_D3D_FVF_ABSENT) {
            elements[count++] = {
                "TEXCOORD", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u,
                layout.texcoord_offset, D3D11_INPUT_PER_VERTEX_DATA, 0u};
        }
        result = presenter->device->CreateInputLayout(
            elements,
            count,
            vertex_blob->GetBufferPointer(),
            vertex_blob->GetBufferSize(),
            &pipeline.input_layout);
    }
    releaseCom(vertex_blob);
    releaseCom(pixel_blob);
    if (FAILED(result)) {
        std::fprintf(
            stderr,
            "recomp d3d presenter: draw pipeline fvf=0x%08X failed "
            "hr=0x%08lX\n",
            static_cast<unsigned>(fvf),
            static_cast<unsigned long>(result));
        releaseCom(pipeline.input_layout);
        releaseCom(pipeline.pixel_shader);
        releaseCom(pipeline.vertex_shader);
        return false;
    }
    std::fprintf(
        stderr,
        "recomp d3d presenter: draw pipeline fvf=0x%08X stride=%u "
        "normal=%d diffuse=%d texcoord=%d\n",
        static_cast<unsigned>(fvf),
        static_cast<unsigned>(layout.stride),
        layout.normal_offset != RECOMP_D3D_FVF_ABSENT
            ? static_cast<int>(layout.normal_offset) : -1,
        layout.diffuse_offset != RECOMP_D3D_FVF_ABSENT
            ? static_cast<int>(layout.diffuse_offset) : -1,
        layout.texcoord_offset != RECOMP_D3D_FVF_ABSENT
            ? static_cast<int>(layout.texcoord_offset) : -1);
    return true;
}

/* Device state every draw shares, independent of vertex format. */
bool ensureSharedDrawState(RecompD3dPresenter *presenter)
{
    if (presenter->draw_shared_ready) {
        return true;
    }
    if (presenter->draw_shared_failed) {
        return false;
    }
    presenter->draw_shared_failed = true;

    HRESULT result;
    D3D11_BUFFER_DESC constant_desc{};
    /* 4x4 transform plus one float4 of draw flags. */
    constant_desc.ByteWidth = 80u;
    constant_desc.Usage = D3D11_USAGE_DYNAMIC;
    constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constant_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = presenter->device->CreateBuffer(
        &constant_desc, nullptr, &presenter->draw_constant_buffer);
    if (FAILED(result)) {
        return false;
    }

    /* The guest culls with its own winding and this seam does not yet track
       render state, so cull nothing rather than silently dropping faces. */
    D3D11_RASTERIZER_DESC rasterizer_desc{};
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.DepthClipEnable = TRUE;
    result = presenter->device->CreateRasterizerState(
        &rasterizer_desc, &presenter->draw_rasterizer_state);
    if (FAILED(result)) {
        return false;
    }

    /* The guest's own sampler state is a separate seam; linear filtering with
       wrap addressing is the common case for this title's textures. */
    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    result = presenter->device->CreateSamplerState(
        &sampler_desc, &presenter->draw_sampler);
    if (FAILED(result)) {
        return false;
    }

    presenter->draw_shared_failed = false;
    presenter->draw_shared_ready = true;
    return true;
}

/* Returns the pipeline for one FVF, building it on first use. A pipeline that
   fails to build is remembered against its own FVF so it is not retried every
   draw and does not affect any other FVF. */
const DrawPipeline *lookupDrawPipeline(
    RecompD3dPresenter *presenter,
    uint32_t fvf)
{
    for (uint32_t i = 0u; i < presenter->draw_pipeline_count; ++i) {
        DrawPipeline &pipeline = presenter->draw_pipelines[i];

        if (pipeline.used && pipeline.fvf == fvf) {
            return pipeline.failed ? nullptr : &pipeline;
        }
    }
    if (presenter->draw_pipeline_count == kDrawPipelineSlots) {
        return nullptr;
    }

    DrawPipeline &pipeline =
        presenter->draw_pipelines[presenter->draw_pipeline_count++];
    pipeline.fvf = fvf;
    pipeline.used = true;
    pipeline.failed = !createDrawPipeline(presenter, fvf, pipeline);
    return pipeline.failed ? nullptr : &pipeline;
}

bool ensureDynamicBuffer(
    RecompD3dPresenter *presenter,
    ID3D11Buffer **buffer,
    UINT &capacity,
    UINT required,
    UINT bind_flags)
{
    if (*buffer != nullptr && capacity >= required) {
        return true;
    }

    UINT size = capacity != 0u ? capacity : 4096u;
    while (size < required) {
        size *= 2u;
    }

    releaseCom(*buffer);
    capacity = 0u;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = size;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = bind_flags;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(presenter->device->CreateBuffer(&desc, nullptr, buffer))) {
        return false;
    }
    capacity = size;
    return true;
}

bool uploadBuffer(
    RecompD3dPresenter *presenter,
    ID3D11Buffer *buffer,
    const void *source,
    size_t size)
{
    D3D11_MAPPED_SUBRESOURCE mapped{};

    if (FAILED(presenter->context->Map(
            buffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped))) {
        return false;
    }
    std::memcpy(mapped.pData, source, size);
    presenter->context->Unmap(buffer, 0u);
    return true;
}

D3D11_COMPARISON_FUNC hostCompareFunc(RecompD3dCompareFunc func)
{
    switch (func) {
    case RECOMP_D3D_COMPARE_NEVER:
        return D3D11_COMPARISON_NEVER;
    case RECOMP_D3D_COMPARE_LESS:
        return D3D11_COMPARISON_LESS;
    case RECOMP_D3D_COMPARE_EQUAL:
        return D3D11_COMPARISON_EQUAL;
    case RECOMP_D3D_COMPARE_GREATER:
        return D3D11_COMPARISON_GREATER;
    case RECOMP_D3D_COMPARE_NOT_EQUAL:
        return D3D11_COMPARISON_NOT_EQUAL;
    case RECOMP_D3D_COMPARE_GREATER_EQUAL:
        return D3D11_COMPARISON_GREATER_EQUAL;
    case RECOMP_D3D_COMPARE_ALWAYS:
        return D3D11_COMPARISON_ALWAYS;
    case RECOMP_D3D_COMPARE_LESS_EQUAL:
    default:
        return D3D11_COMPARISON_LESS_EQUAL;
    }
}

/* Returns the depth-stencil state for one decoded guest depth state, creating
   it on first use. Returns null when the cache is full or creation fails, so
   the caller can fall back to the host default rather than skip the draw. */
ID3D11DepthStencilState *lookupDepthState(
    RecompD3dPresenter *presenter,
    const RecompD3dDepthState &depth)
{
    for (uint32_t i = 0u; i < presenter->depth_state_count; ++i) {
        DepthStateEntry &entry = presenter->depth_states[i];

        if (entry.used && entry.test_enable == depth.depth_test_enable &&
            entry.write_enable == depth.depth_write_enable &&
            entry.func == depth.depth_func) {
            return entry.state;
        }
    }
    if (presenter->depth_state_count == kDepthStateSlots) {
        return nullptr;
    }

    D3D11_DEPTH_STENCIL_DESC desc{};
    desc.DepthEnable = depth.depth_test_enable ? TRUE : FALSE;
    desc.DepthWriteMask = depth.depth_write_enable
        ? D3D11_DEPTH_WRITE_MASK_ALL
        : D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = hostCompareFunc(depth.depth_func);
    desc.StencilEnable = FALSE;

    ID3D11DepthStencilState *state = nullptr;
    if (FAILED(presenter->device->CreateDepthStencilState(&desc, &state))) {
        return nullptr;
    }

    DepthStateEntry &entry =
        presenter->depth_states[presenter->depth_state_count++];
    entry.used = true;
    entry.test_enable = depth.depth_test_enable;
    entry.write_enable = depth.depth_write_enable;
    entry.func = depth.depth_func;
    entry.state = state;
    std::fprintf(
        stderr,
        "recomp d3d presenter: depth state test=%d write=%d func=%d\n",
        depth.depth_test_enable ? 1 : 0,
        depth.depth_write_enable ? 1 : 0,
        static_cast<int>(depth.depth_func));
    return state;
}

D3D11_BLEND hostBlendFactor(RecompD3dBlendFactor factor, bool alpha_channel)
{
    switch (factor) {
    case RECOMP_D3D_BLEND_ZERO:
        return D3D11_BLEND_ZERO;
    case RECOMP_D3D_BLEND_SRC_COLOR:
        /* The alpha blend equation may only name alpha operands. */
        return alpha_channel ? D3D11_BLEND_SRC_ALPHA : D3D11_BLEND_SRC_COLOR;
    case RECOMP_D3D_BLEND_INV_SRC_COLOR:
        return alpha_channel ? D3D11_BLEND_INV_SRC_ALPHA
                             : D3D11_BLEND_INV_SRC_COLOR;
    case RECOMP_D3D_BLEND_SRC_ALPHA:
        return D3D11_BLEND_SRC_ALPHA;
    case RECOMP_D3D_BLEND_INV_SRC_ALPHA:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case RECOMP_D3D_BLEND_DST_ALPHA:
        return D3D11_BLEND_DEST_ALPHA;
    case RECOMP_D3D_BLEND_INV_DST_ALPHA:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case RECOMP_D3D_BLEND_DST_COLOR:
        return alpha_channel ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_DEST_COLOR;
    case RECOMP_D3D_BLEND_INV_DST_COLOR:
        return alpha_channel ? D3D11_BLEND_INV_DEST_ALPHA
                             : D3D11_BLEND_INV_DEST_COLOR;
    case RECOMP_D3D_BLEND_SRC_ALPHA_SATURATE:
        return D3D11_BLEND_SRC_ALPHA_SAT;
    case RECOMP_D3D_BLEND_ONE:
    default:
        return D3D11_BLEND_ONE;
    }
}

D3D11_BLEND_OP hostBlendOp(RecompD3dBlendOp op)
{
    switch (op) {
    case RECOMP_D3D_BLEND_OP_MIN:
        return D3D11_BLEND_OP_MIN;
    case RECOMP_D3D_BLEND_OP_MAX:
        return D3D11_BLEND_OP_MAX;
    case RECOMP_D3D_BLEND_OP_SUBTRACT:
        return D3D11_BLEND_OP_SUBTRACT;
    case RECOMP_D3D_BLEND_OP_REVERSE_SUBTRACT:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    case RECOMP_D3D_BLEND_OP_ADD:
    default:
        return D3D11_BLEND_OP_ADD;
    }
}

/* Returns the blend state for one decoded guest blend state, creating it on
   first use. Null falls back to the host default, which is blending off. */
/* Xbox block-compressed formats map straight onto the host equivalents, and
   both store 4x4 blocks in the same order, so the guest bytes upload as-is.
   The uncompressed formats need unswizzling first. */
DXGI_FORMAT hostTextureFormat(uint32_t format_byte)
{
    switch (format_byte) {
    case RECOMP_D3D_TEXTURE_FORMAT_DXT1:
        return DXGI_FORMAT_BC1_UNORM;
    case RECOMP_D3D_TEXTURE_FORMAT_DXT3:
        return DXGI_FORMAT_BC2_UNORM;
    case RECOMP_D3D_TEXTURE_FORMAT_DXT5:
        return DXGI_FORMAT_BC3_UNORM;
    case RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case RECOMP_D3D_TEXTURE_FORMAT_A8:
        return DXGI_FORMAT_A8_UNORM;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

bool isSwizzledTextureFormat(uint32_t format_byte)
{
    return format_byte == RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8 ||
        format_byte == RECOMP_D3D_TEXTURE_FORMAT_A8;
}

/* Draw-time tally of the formats a draw actually sampled, keyed by format
   byte, plus how many draws wanted a texture and got none. */
struct DrawTextureTally {
    uint32_t textured[256];
    uint32_t rejected[256];
    uint32_t untextured;
};

DrawTextureTally draw_texture_tally;

void recompD3dPresenterCountDrawTexture(
    const RecompD3dPresenterDrawCommand &draw,
    bool sampled)
{
    if (!draw.has_texture) {
        ++draw_texture_tally.untextured;
        return;
    }
    if (sampled) {
        ++draw_texture_tally.textured[draw.texture.format_byte & 0xffu];
    } else {
        ++draw_texture_tally.rejected[draw.texture.format_byte & 0xffu];
    }
}

ID3D11ShaderResourceView *lookupTexture(
    RecompD3dPresenter *presenter,
    const RecompD3dPresenterDrawCommand &draw)
{
    const RecompD3dTextureDesc &desc = draw.texture;

    for (uint32_t i = 0u; i < presenter->texture_count; ++i) {
        TextureEntry &entry = presenter->textures[i];

        if (entry.used && entry.data == desc.data &&
            entry.format_byte == desc.format_byte &&
            entry.width == desc.width && entry.height == desc.height) {
            return entry.view;
        }
    }
    if (presenter->texture_count == kTextureSlots) {
        return nullptr;
    }

    const DXGI_FORMAT format = hostTextureFormat(desc.format_byte);
    if (format == DXGI_FORMAT_UNKNOWN) {
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = desc.width;
    texture_desc.Height = desc.height;
    texture_desc.MipLevels = 1u;
    texture_desc.ArraySize = 1u;
    texture_desc.Format = format;
    texture_desc.SampleDesc.Count = 1u;
    texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initial{};
    std::vector<uint8_t> unswizzled;

    if (isSwizzledTextureFormat(desc.format_byte)) {
        const uint32_t texel_bytes = desc.bits_per_pixel / 8u;

        unswizzled.resize(
            static_cast<size_t>(desc.width) * desc.height * texel_bytes);
        if (!recomp_d3d_texture_unswizzle(
                static_cast<const uint8_t *>(draw.texture_bytes),
                unswizzled.data(),
                desc.width,
                desc.height,
                texel_bytes)) {
            return nullptr;
        }
        initial.pSysMem = unswizzled.data();
        initial.SysMemPitch = desc.width * texel_bytes;
    } else {
        const uint32_t blocks_wide = (desc.width + 3u) / 4u;
        const uint32_t block_bytes =
            desc.format_byte == RECOMP_D3D_TEXTURE_FORMAT_DXT1 ? 8u : 16u;

        initial.pSysMem = draw.texture_bytes;
        initial.SysMemPitch = blocks_wide * block_bytes;
    }

    ID3D11Texture2D *texture = nullptr;
    if (FAILED(presenter->device->CreateTexture2D(
            &texture_desc, &initial, &texture))) {
        return nullptr;
    }

    ID3D11ShaderResourceView *view = nullptr;
    const HRESULT view_result =
        presenter->device->CreateShaderResourceView(texture, nullptr, &view);
    releaseCom(texture);
    if (FAILED(view_result)) {
        return nullptr;
    }

    TextureEntry &entry = presenter->textures[presenter->texture_count++];
    entry.used = true;
    entry.data = desc.data;
    entry.format_byte = desc.format_byte;
    entry.width = desc.width;
    entry.height = desc.height;
    entry.view = view;
    return view;
}

ID3D11BlendState *lookupBlendState(
    RecompD3dPresenter *presenter,
    const RecompD3dBlendState &blend)
{
    for (uint32_t i = 0u; i < presenter->blend_state_count; ++i) {
        BlendStateEntry &entry = presenter->blend_states[i];

        if (entry.used && entry.enable == blend.blend_enable &&
            entry.src == blend.src_factor && entry.dst == blend.dst_factor &&
            entry.op == blend.op) {
            return entry.state;
        }
    }
    if (presenter->blend_state_count == kBlendStateSlots) {
        return nullptr;
    }

    D3D11_BLEND_DESC desc{};
    D3D11_RENDER_TARGET_BLEND_DESC &target = desc.RenderTarget[0];
    target.BlendEnable = blend.blend_enable ? TRUE : FALSE;
    target.SrcBlend = hostBlendFactor(blend.src_factor, false);
    target.DestBlend = hostBlendFactor(blend.dst_factor, false);
    target.BlendOp = hostBlendOp(blend.op);
    target.SrcBlendAlpha = hostBlendFactor(blend.src_factor, true);
    target.DestBlendAlpha = hostBlendFactor(blend.dst_factor, true);
    target.BlendOpAlpha = hostBlendOp(blend.op);
    target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState *state = nullptr;
    if (FAILED(presenter->device->CreateBlendState(&desc, &state))) {
        return nullptr;
    }

    BlendStateEntry &entry =
        presenter->blend_states[presenter->blend_state_count++];
    entry.used = true;
    entry.enable = blend.blend_enable;
    entry.src = blend.src_factor;
    entry.dst = blend.dst_factor;
    entry.op = blend.op;
    entry.state = state;
    std::fprintf(
        stderr,
        "recomp d3d presenter: blend state enable=%d src=%d dst=%d op=%d\n",
        blend.blend_enable ? 1 : 0,
        static_cast<int>(blend.src_factor),
        static_cast<int>(blend.dst_factor),
        static_cast<int>(blend.op));
    return state;
}

RecompD3dPresenterError submitDraw(
    RecompD3dPresenter *presenter,
    const RecompD3dPresenterDrawCommand &draw)
{
    if (draw.vertex_bytes == nullptr || draw.index_bytes == nullptr ||
        draw.vertex_stride == 0u || draw.vertex_count == 0u ||
        draw.index_count == 0u || !draw.has_transform) {
        return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    }

    D3D11_PRIMITIVE_TOPOLOGY topology;
    switch (draw.primitive_type) {
    case 5u:
        topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case 6u:
        topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
    default:
        /* D3D11 has no triangle fan. Fans are converted to a list by the
           caller-side index expansion below. */
        topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    }

    if (!ensureSharedDrawState(presenter)) {
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }

    const DrawPipeline *pipeline = lookupDrawPipeline(presenter, draw.fvf);
    if (pipeline == nullptr) {
        return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    }

    const UINT vertex_size = draw.vertex_stride * draw.vertex_count;
    if (!ensureDynamicBuffer(
            presenter,
            &presenter->draw_vertex_buffer,
            presenter->draw_vertex_capacity,
            vertex_size,
            D3D11_BIND_VERTEX_BUFFER) ||
        !uploadBuffer(
            presenter,
            presenter->draw_vertex_buffer,
            draw.vertex_bytes,
            vertex_size)) {
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }

    /* Triangle fans are expanded into a triangle list here because D3D11
       dropped fan topology; every other supported primitive passes through
       with its indices unchanged. */
    const auto *source_indices =
        static_cast<const uint16_t *>(draw.index_bytes);
    UINT draw_index_count = draw.index_count;
    static uint16_t fan_indices[3u * 4096u];
    const void *upload_indices = draw.index_bytes;

    if (draw.primitive_type == 7u) {
        if (draw.triangle_count > 4096u) {
            return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
        }
        for (uint32_t i = 0u; i < draw.triangle_count; ++i) {
            fan_indices[i * 3u + 0u] = source_indices[0];
            fan_indices[i * 3u + 1u] = source_indices[i + 1u];
            fan_indices[i * 3u + 2u] = source_indices[i + 2u];
        }
        draw_index_count = draw.triangle_count * 3u;
        upload_indices = fan_indices;
    }

    const UINT index_size = draw_index_count * 2u;
    ID3D11ShaderResourceView *texture_view =
        draw.has_texture ? lookupTexture(presenter, draw) : nullptr;
    /* Observation only: bind counts say what the guest selected, not what a
       draw actually consumed, and only the latter can explain the frame. */
    recompD3dPresenterCountDrawTexture(draw, texture_view != nullptr);
    float draw_constants[20];
    std::memcpy(draw_constants, draw.transform, sizeof draw.transform);
    draw_constants[16] = texture_view != nullptr ? 1.0f : 0.0f;
    draw_constants[17] = 0.0f;
    draw_constants[18] = 0.0f;
    draw_constants[19] = 0.0f;

    if (!ensureDynamicBuffer(
            presenter,
            &presenter->draw_index_buffer,
            presenter->draw_index_capacity,
            index_size,
            D3D11_BIND_INDEX_BUFFER) ||
        !uploadBuffer(
            presenter,
            presenter->draw_index_buffer,
            upload_indices,
            index_size) ||
        !uploadBuffer(
            presenter,
            presenter->draw_constant_buffer,
            draw_constants,
            sizeof draw_constants)) {
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }

    const UINT stride = draw.vertex_stride;
    const UINT offset = 0u;
    presenter->context->IASetInputLayout(pipeline->input_layout);
    presenter->context->IASetVertexBuffers(
        0u, 1u, &presenter->draw_vertex_buffer, &stride, &offset);
    presenter->context->IASetIndexBuffer(
        presenter->draw_index_buffer, DXGI_FORMAT_R16_UINT, 0u);
    presenter->context->IASetPrimitiveTopology(topology);
    presenter->context->VSSetShader(pipeline->vertex_shader, nullptr, 0u);
    presenter->context->VSSetConstantBuffers(
        0u, 1u, &presenter->draw_constant_buffer);
    presenter->context->PSSetShader(pipeline->pixel_shader, nullptr, 0u);
    presenter->context->PSSetConstantBuffers(
        0u, 1u, &presenter->draw_constant_buffer);
    presenter->context->PSSetShaderResources(0u, 1u, &texture_view);
    presenter->context->PSSetSamplers(0u, 1u, &presenter->draw_sampler);
    presenter->context->RSSetState(presenter->draw_rasterizer_state);
    presenter->context->OMSetDepthStencilState(
        lookupDepthState(presenter, draw.depth), 0u);
    {
        const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        presenter->context->OMSetBlendState(
            lookupBlendState(presenter, draw.blend),
            blend_factor,
            0xffffffffu);
    }
    presenter->context->OMSetRenderTargets(
        1u, &presenter->render_target_view, presenter->depth_view);
    presenter->context->DrawIndexed(draw_index_count, 0u, 0);
    ++presenter->draw_count;

    /* Back-buffer dumps proved the rendered content alternates vertically by
       about 32 rows every other frame, and nothing in this file derives a
       coordinate from a frame counter. So the offset must arrive inside the
       guest transform. Log the translation row per draw for a bounded window
       so the alternation can be attributed to guest state rather than the
       host. Opt-in and off by default. */
    {
        static const char *ytrace = std::getenv("RECOMP_D3D_YTRACE");
        static unsigned ytrace_lines;

        /* One line per draw drowns in a single frame: a frame issues hundreds
           of draws, so a flat budget never reaches a second frame. The
           question is how one draw differs ACROSS frames, so sample the same
           ordinal draw of each present instead. */
        static unsigned ytrace_last_present = 0xffffffffu;
        static unsigned ytrace_ordinal;
        const char *ord_text = std::getenv("RECOMP_D3D_YTRACE_DRAW");
        const unsigned want_ordinal = ord_text != nullptr
            ? static_cast<unsigned>(std::strtoul(ord_text, nullptr, 10))
            : 1u;

        if (presenter->present_count != ytrace_last_present) {
            ytrace_last_present = presenter->present_count;
            ytrace_ordinal = 0u;
        }
        ++ytrace_ordinal;

        if (ytrace != nullptr && ytrace_lines < 400u &&
            ytrace_ordinal == want_ordinal) {
            const float *m = draw.transform;
            float p[3] = {0.0f, 0.0f, 0.0f};

            if (draw.vertex_stride >= sizeof p) {
                std::memcpy(p, draw.vertex_bytes, sizeof p);
            }
            ++ytrace_lines;
            std::fprintf(
                stderr,
                "recomp d3d ytrace: present=%u draw=%u fvf=0x%08X "
                "m13=%g m5=%g m1=%g m9=%g v0=(%g %g %g) idx=%u\n",
                static_cast<unsigned>(presenter->present_count),
                static_cast<unsigned>(presenter->draw_count),
                static_cast<unsigned>(draw.fvf),
                m[13], m[5], m[1], m[9], p[0], p[1], p[2],
                static_cast<unsigned>(draw_index_count));
        }
    }

    if (!presenter->first_draw_reported) {
        presenter->first_draw_reported = true;
        std::fprintf(
            stderr,
            "recomp d3d presenter: first draw prim=%u indices=%u "
            "triangles=%u vertices=%u stride=%u fvf=0x%08X\n",
            static_cast<unsigned>(draw.primitive_type),
            static_cast<unsigned>(draw_index_count),
            static_cast<unsigned>(draw.triangle_count),
            static_cast<unsigned>(draw.vertex_count),
            static_cast<unsigned>(draw.vertex_stride),
            static_cast<unsigned>(draw.fvf));
        /* The draw submits but the frame stays black, so report where these
           vertices actually land in clip space. Anything outside |x|,|y| <= w
           or w <= 0 is clipped away and explains a black frame without any
           API error. */
        const float *m = draw.transform;
        std::fprintf(
            stderr,
            "recomp d3d presenter: wvp [%g %g %g %g][%g %g %g %g]"
            "[%g %g %g %g][%g %g %g %g]\n",
            m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
        const unsigned probe = draw.vertex_count < 3u ? draw.vertex_count : 3u;
        for (unsigned v = 0u; v < probe; ++v) {
            float p[3];
            std::memcpy(
                p,
                static_cast<const unsigned char *>(draw.vertex_bytes) +
                    static_cast<size_t>(v) * draw.vertex_stride,
                sizeof p);
            const float cx = p[0] * m[0] + p[1] * m[4] + p[2] * m[8] + m[12];
            const float cy = p[0] * m[1] + p[1] * m[5] + p[2] * m[9] + m[13];
            const float cz = p[0] * m[2] + p[1] * m[6] + p[2] * m[10] + m[14];
            const float cw = p[0] * m[3] + p[1] * m[7] + p[2] * m[11] + m[15];
            std::fprintf(
                stderr,
                "recomp d3d presenter: v%u obj=(%g %g %g) clip=(%g %g %g %g)%s\n",
                v, p[0], p[1], p[2], cx, cy, cz, cw,
                (cw > 0.0f && cz >= 0.0f && cz <= cw &&
                 cx >= -cw && cx <= cw && cy >= -cw && cy <= cw)
                    ? " inside" : " OUTSIDE");
        }
    }
    return FAILED(presenter->device->GetDeviceRemovedReason())
        ? RECOMP_D3D_PRESENTER_HOST_FAILURE
        : RECOMP_D3D_PRESENTER_OK;
}

bool pumpMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

/* Writes one BMP of the back buffer after the requested present, when
   RECOMP_D3D_FRAME_DUMP names a path and RECOMP_D3D_FRAME_DUMP_AT names a
   present index. BMP keeps this dependency-free; the gate does the analysis. */
void dumpBackBufferOnce(RecompD3dPresenter *presenter)
{
    static unsigned dumped = 0u;

    const char *path = std::getenv("RECOMP_D3D_FRAME_DUMP");
    if (path == nullptr) {
        return;
    }
    /* A single dump cannot answer whether content moves between frames, only
       whether one frame is correct. RECOMP_D3D_FRAME_DUMP_COUNT captures a
       burst of consecutive presents so successive back buffers can be
       compared against each other. Default 1 keeps existing gates identical. */
    const char *count_text = std::getenv("RECOMP_D3D_FRAME_DUMP_COUNT");
    const unsigned count = count_text != nullptr
        ? static_cast<unsigned>(std::strtoul(count_text, nullptr, 10))
        : 1u;
    if (dumped >= (count == 0u ? 1u : count)) {
        return;
    }
    const char *at_text = std::getenv("RECOMP_D3D_FRAME_DUMP_AT");
    const unsigned at = at_text != nullptr
        ? static_cast<unsigned>(std::strtoul(at_text, nullptr, 10))
        : 1u;
    if (presenter->present_count < at) {
        return;
    }
    const unsigned dump_index = dumped;
    ++dumped;

    /* One name per frame in a burst; the single-dump case keeps the exact
       path it always used so existing gates and receipts still match. */
    char burst_path[1024];
    if (count > 1u) {
        std::snprintf(
            burst_path, sizeof burst_path, "%s.%03u.bmp", path, dump_index);
        path = burst_path;
    }

    ID3D11Texture2D *back_buffer = nullptr;
    if (FAILED(presenter->swap_chain->GetBuffer(
            0u, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void **>(&back_buffer)))) {
        return;
    }
    D3D11_TEXTURE2D_DESC desc{};
    back_buffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0u;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0u;

    ID3D11Texture2D *staging = nullptr;
    if (FAILED(presenter->device->CreateTexture2D(&desc, nullptr, &staging))) {
        back_buffer->Release();
        return;
    }
    presenter->context->CopyResource(staging, back_buffer);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(presenter->context->Map(
            staging, 0u, D3D11_MAP_READ, 0u, &mapped))) {
        const unsigned width = desc.Width;
        const unsigned height = desc.Height;
        const unsigned row_bytes = width * 3u;
        const unsigned padded = (row_bytes + 3u) & ~3u;
        const unsigned image_bytes = padded * height;

        if (FILE *file = std::fopen(path, "wb")) {
            unsigned char header[54] = {0};
            const unsigned total = 54u + image_bytes;

            header[0] = 'B'; header[1] = 'M';
            std::memcpy(header + 2, &total, 4);
            const unsigned offset = 54u;
            std::memcpy(header + 10, &offset, 4);
            const unsigned info_size = 40u;
            std::memcpy(header + 14, &info_size, 4);
            std::memcpy(header + 18, &width, 4);
            std::memcpy(header + 22, &height, 4);
            const unsigned short planes = 1u;
            std::memcpy(header + 26, &planes, 2);
            const unsigned short bpp = 24u;
            std::memcpy(header + 28, &bpp, 2);
            std::memcpy(header + 34, &image_bytes, 4);
            std::fwrite(header, 1, sizeof header, file);

            /* BMP rows run bottom-up. */
            for (unsigned y = 0u; y < height; ++y) {
                const unsigned char *row =
                    static_cast<const unsigned char *>(mapped.pData) +
                    static_cast<size_t>(height - 1u - y) * mapped.RowPitch;
                unsigned written = 0u;

                for (unsigned x = 0u; x < width; ++x) {
                    /* Back buffer is B8G8R8A8, which already matches BMP order. */
                    std::fwrite(row + x * 4u, 1, 3, file);
                    written += 3u;
                }
                const unsigned char pad[3] = {0, 0, 0};
                if (padded > written) {
                    std::fwrite(pad, 1, padded - written, file);
                }
            }
            std::fclose(file);
            std::fprintf(
                stderr,
                "recomp d3d presenter: frame dump path=%s size=%ux%u present=%u\n",
                path, width, height,
                static_cast<unsigned>(presenter->present_count));
        }
        presenter->context->Unmap(staging, 0u);
    }
    staging->Release();
    back_buffer->Release();
}

RecompD3dPresenterError submitPresent(
    RecompD3dPresenter *presenter,
    const RecompD3dPresenterPresentCommand &present)
{
    if (present.effective_flags != 5u || present.swap_counter == 0u) {
        return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    }
    /* Three distinct conditions used to collapse into one HOST_FAILURE code,
       so a stop here named the symptom and not the cause. Name each one. */
    {
        const bool pumped = pumpMessages();
        if (!pumped || !IsWindow(presenter->window)) {
            std::fprintf(
                stderr,
                "recomp d3d presenter: present precondition failed "
                "wm_quit=%d is_window=%d present_count=%u swap=%u "
                "last_error=%lu\n",
                pumped ? 0 : 1,
                IsWindow(presenter->window) ? 1 : 0,
                static_cast<unsigned>(presenter->present_count),
                static_cast<unsigned>(present.swap_counter),
                GetLastError());
            return RECOMP_D3D_PRESENTER_HOST_FAILURE;
        }
    }

    /* SyncInterval, not the flag, is what paces a present. Passing 0 here
       unconditionally meant the throttled path was never actually throttled:
       every frame was retired immediately and only the blocking behaviour
       changed. Pace to one refresh unless immediate presenting is asked for. */
    const HRESULT present_result = presenter->swap_chain->Present(
        immediate_present ? 0u : 1u,
        immediate_present ? DXGI_PRESENT_DO_NOT_WAIT : 0u);
    if (FAILED(present_result)) {
        std::fprintf(
            stderr,
            "recomp d3d presenter: Present failed hr=0x%08lX "
            "removed_reason=0x%08lX present_count=%u swap=%u\n",
            static_cast<unsigned long>(present_result),
            static_cast<unsigned long>(
                presenter->device->GetDeviceRemovedReason()),
            static_cast<unsigned>(presenter->present_count),
            static_cast<unsigned>(present.swap_counter));
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }
    ++presenter->present_count;

    /* Screen-scraping the presenter window proved unreliable as a gate: the
       captured rectangle is whatever is topmost at that screen position, so a
       run can "pass" on desktop pixels. Reading the back buffer here measures
       what this program actually rendered. Opt-in, and off by default. */
    dumpBackBufferOnce(presenter);

    if (!presenter->first_present_reported) {
        ShowWindow(presenter->window, SW_SHOW);
        UpdateWindow(presenter->window);
        if (!pumpMessages() || !IsWindow(presenter->window)) {
            return RECOMP_D3D_PRESENTER_HOST_FAILURE;
        }
        RECT client_rect{};
        if (!GetClientRect(presenter->window, &client_rect) ||
            !IsWindowVisible(presenter->window)) {
            return RECOMP_D3D_PRESENTER_HOST_FAILURE;
        }
        std::fprintf(
            stderr,
            "recomp d3d presenter: creation/first-present hwnd=%p "
            "client=%ldx%ld visible=1 driver=%s create_hr=0x%08lX "
            "present_hr=0x%08lX count=%u\n",
            static_cast<void *>(presenter->window),
            client_rect.right - client_rect.left,
            client_rect.bottom - client_rect.top,
            presenter->driver_name,
            static_cast<unsigned long>(presenter->create_result),
            static_cast<unsigned long>(present_result),
            static_cast<unsigned>(presenter->present_count));
        presenter->first_present_reported = true;
    }
    return RECOMP_D3D_PRESENTER_OK;
}

} // namespace

RecompD3dPresenterError recomp_d3d_presenter_create(
    const RecompD3dPresenterConfig *config,
    RecompD3dPresenter **presenter)
{
    if (config == nullptr || presenter == nullptr || !configSupported(*config)) {
        return RECOMP_D3D_PRESENTER_INVALID_ARGUMENT;
    }
    if (*presenter != nullptr || active_presenter != nullptr) {
        return RECOMP_D3D_PRESENTER_ALREADY_INITIALIZED;
    }

    RecompD3dPresenter *created = new (std::nothrow) RecompD3dPresenter{};
    if (created == nullptr) {
        return RECOMP_D3D_PRESENTER_OUT_OF_MEMORY;
    }
    created->config = *config;
    created->owner_thread = GetCurrentThreadId();
    if (!createWindow(created)) {
        releasePresenter(created);
        delete created;
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }
    const HRESULT create_result = createGraphics(created);
    if (FAILED(create_result)) {
        releasePresenter(created);
        delete created;
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }

    active_presenter = created;
    *presenter = created;
    return RECOMP_D3D_PRESENTER_OK;
}

RecompD3dPresenterError recomp_d3d_presenter_submit(
    RecompD3dPresenter *presenter,
    const RecompD3dPresenterCommand *command)
{
    if (presenter == nullptr || presenter != active_presenter) {
        return RECOMP_D3D_PRESENTER_NOT_INITIALIZED;
    }
    if (GetCurrentThreadId() != presenter->owner_thread) {
        return RECOMP_D3D_PRESENTER_WRONG_THREAD;
    }
    if (command == nullptr) {
        return RECOMP_D3D_PRESENTER_INVALID_ARGUMENT;
    }

    switch (command->type) {
    case RECOMP_D3D_PRESENTER_COMMAND_CLEAR:
        return submitClear(presenter, command->data.clear);
    case RECOMP_D3D_PRESENTER_COMMAND_PRESENT:
        return submitPresent(presenter, command->data.present);
    case RECOMP_D3D_PRESENTER_COMMAND_DRAW:
        return submitDraw(presenter, command->data.draw);
    default:
        return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    }
}

RecompD3dPresenterError recomp_d3d_presenter_destroy(
    RecompD3dPresenter **presenter)
{
    if (presenter == nullptr) {
        return RECOMP_D3D_PRESENTER_INVALID_ARGUMENT;
    }
    if (*presenter == nullptr || *presenter != active_presenter) {
        return RECOMP_D3D_PRESENTER_NOT_INITIALIZED;
    }
    if (GetCurrentThreadId() != (*presenter)->owner_thread) {
        return RECOMP_D3D_PRESENTER_WRONG_THREAD;
    }

    RecompD3dPresenter *destroyed = *presenter;
    active_presenter = nullptr;
    *presenter = nullptr;
    releasePresenter(destroyed);
    delete destroyed;
    return RECOMP_D3D_PRESENTER_OK;
}

void recomp_d3d_presenter_set_immediate_present(bool enabled)
{
    immediate_present = enabled;
}

void recomp_d3d_presenter_report_draw_textures(void)
{
    for (uint32_t format = 0u; format < 256u; ++format) {
        const uint32_t sampled = draw_texture_tally.textured[format];
        const uint32_t rejected = draw_texture_tally.rejected[format];

        if (sampled == 0u && rejected == 0u) {
            continue;
        }
        std::fprintf(
            stderr,
            "recomp d3d presenter: draw texture fmt=0x%02x sampled=%u "
            "rejected=%u\n",
            format,
            sampled,
            rejected);
    }
    std::fprintf(
        stderr,
        "recomp d3d presenter: draw texture untextured=%u cached=%u\n",
        draw_texture_tally.untextured,
        active_presenter != nullptr ? active_presenter->texture_count : 0u);
}
