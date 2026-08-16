#include "d3d_presenter.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>

#include <cmath>
#include <cstdio>
#include <new>

namespace {

constexpr wchar_t kWindowClassName[] = L"DOAXBVRecompPresenterWindow";
constexpr wchar_t kWindowTitle[] = L"DOAXBV Recomp";

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
    const char *driver_name = "unknown";
    HRESULT create_result = E_FAIL;
    uint32_t present_count = 0;
    bool first_present_reported = false;
};

namespace {

RecompD3dPresenter *active_presenter;

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

RecompD3dPresenterError submitPresent(
    RecompD3dPresenter *presenter,
    const RecompD3dPresenterPresentCommand &present)
{
    if (present.effective_flags != 5u || present.swap_counter == 0u) {
        return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    }
    if (!pumpMessages() || !IsWindow(presenter->window)) {
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }

    const HRESULT present_result = presenter->swap_chain->Present(0u, 0u);
    if (FAILED(present_result)) {
        return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    }
    ++presenter->present_count;

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
