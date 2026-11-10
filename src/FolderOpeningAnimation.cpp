#include <common.h>
#include "FolderOpeningAnimation.h"
#include "main.h"
#include <atlbase.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <atomic>
#include <algorithm>
#include <type_traits>

#pragma comment(lib, "D3d11.lib")
#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "Dcomp.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace filespacer {
namespace {
const wchar_t animationClass[] = L"FileSpacer Folder Opening Animation";
struct OpeningGraphics {
    CComPtr<ID3D11Device> d3d;
    CComPtr<IDCompositionDesktopDevice> composition;
    CComPtr<IDCompositionDevice> health;

    bool initialize() {
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                &d3d, nullptr, nullptr))) return false;
        CComQIPtr<IDXGIDevice> dxgi(d3d);
        CComPtr<ID2D1Device> drawing;
        const D2D1_CREATION_PROPERTIES properties = {
            D2D1_THREADING_MODE_MULTI_THREADED, D2D1_DEBUG_LEVEL_NONE,
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE};
        if (!dxgi || FAILED(D2D1CreateDevice(dxgi, &properties, &drawing))
                || FAILED(DCompositionCreateDevice2(drawing, IID_PPV_ARGS(&composition)))
                || FAILED(composition->QueryInterface(IID_PPV_ARGS(&health)))) return false;
        return true;
    }
};
std::shared_ptr<OpeningGraphics> graphics;
std::atomic_bool preparing{false};

DWORD WINAPI prepareGraphics(void *) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(initialized)) {
        try {
            auto candidate = std::make_shared<OpeningGraphics>();
            if (candidate->initialize()) std::atomic_store(&graphics, candidate);
        } catch (...) { /* Optional effect: opening folders must remain available. */ }
        CoUninitialize();
    }
    unlockProcess();
    return 0;
}

bool animationsEnabled() {
    BOOL enabled = FALSE;
    return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0) && enabled;
}
} // namespace

struct FolderOpeningAnimation::State {
    std::shared_ptr<OpeningGraphics> resources;
    CComPtr<IDCompositionTarget> target;
    CComPtr<IDCompositionVisual2> visual;
    CComPtr<IDCompositionSurface> pattern;
    std::unique_ptr<std::remove_pointer<HWND>::type, decltype(&DestroyWindow)>
        overlay{nullptr, DestroyWindow};
    HWND window = nullptr;
    HWND foreground = nullptr;
    LONG_PTR identity = 0;

    bool current() const {
        return window && IsWindow(window)
            && GetWindowLongPtrW(window, GWLP_USERDATA) == identity;
    }

    void finish(bool activate) {
        if (overlay) KillTimer(overlay.get(), 1);
        const HWND destination = current() ? window : nullptr;
        const HWND previousForeground = foreground;
        window = nullptr;
        auto finishedOverlay = std::move(overlay);
        if (finishedOverlay) SetWindowLongPtrW(finishedOverlay.get(), GWLP_USERDATA, 0);
        CComPtr<IDCompositionTarget> finishedTarget;
        finishedTarget.Attach(target.Detach());
        visual.Release();
        pattern.Release();
        resources.reset();
        // Clear the callback state before activation can reenter and close its owner.
        if (destination) {
            const HWND active = GetForegroundWindow();
            const bool restoreFocus = activate && (active == destination || active == previousForeground || !active);
            // The window has been laid out and can paint during the animation.
            const BOOL disabled = FALSE;
            DwmSetWindowAttribute(destination, DWMWA_CLOAK, &disabled, sizeof(disabled));
            DwmSetWindowAttribute(destination, DWMWA_TRANSITIONS_FORCEDISABLED, &disabled, sizeof(disabled));
            finishedOverlay.reset();
            if (restoreFocus) SetForegroundWindow(destination);
        }
    }

    ~State() { finish(false); }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto self = reinterpret_cast<State *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<State *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self && message == WM_TIMER && wParam == 1) {
            self->finish(true);
            return 0;
        }
        if (self && message == WM_PAINT) {
            PAINTSTRUCT paint = {};
            BeginPaint(hwnd, &paint);
            EndPaint(hwnd, &paint);
            BOOL valid = FALSE;
            if (!self->resources || FAILED(self->resources->health->CheckDeviceState(&valid)) || !valid)
                self->finish(true);
            return 0;
        }
        if (message == WM_NCHITTEST) return HTTRANSPARENT;
        if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    bool animate(IDCompositionAnimation **out, float from, float to, LARGE_INTEGER start) {
        CComPtr<IDCompositionAnimation> animation;
        if (FAILED(resources->composition->CreateAnimation(&animation))
                || FAILED(animation->AddCubic(0.0, from, (to - from) * 10.0f, 0.0f, 0.0f))
                || FAILED(animation->End(0.1, to))
                || FAILED(animation->SetAbsoluteBeginTime(start))) return false;
        *out = animation.Detach();
        return true;
    }

    bool drawPattern(UINT width, UINT height) {
        if (FAILED(resources->composition->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                DXGI_ALPHA_MODE_PREMULTIPLIED, &pattern))) return false;
        CComPtr<ID2D1DeviceContext> context;
        POINT offset = {};
        if (FAILED(pattern->BeginDraw(nullptr, IID_PPV_ARGS(&context), &offset))) return false;
        CComPtr<ID2D1Bitmap> dots;
        CComPtr<ID2D1BitmapBrush> brush;
        const UINT32 pixels[] = {0xFF999999, 0, 0, 0xFF999999};
        const D2D1_BITMAP_PROPERTIES format = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        HRESULT result = context->CreateBitmap(D2D1::SizeU(2, 2), pixels, 2 * sizeof(UINT32), format, &dots);
        if (SUCCEEDED(result)) {
            const D2D1_BITMAP_BRUSH_PROPERTIES tiling = D2D1::BitmapBrushProperties(
                D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
            const D2D1_BRUSH_PROPERTIES properties = D2D1::BrushProperties(1.0f,
                D2D1::Matrix3x2F::Scale(3.0f, 3.0f));
            result = context->CreateBitmapBrush(dots, tiling, properties, &brush);
        }
        if (SUCCEEDED(result)) {
            context->SetTransform(D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x), static_cast<float>(offset.y)));
            context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
            context->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width), static_cast<float>(height)), brush);
        }
        const HRESULT ended = pattern->EndDraw();
        return SUCCEEDED(result) && SUCCEEDED(ended);
    }

    bool addEdge(IDCompositionAnimation *width, IDCompositionAnimation *height,
            IDCompositionAnimation *innerWidth, IDCompositionAnimation *innerHeight, unsigned edge) {
        CComPtr<IDCompositionVisual2> line;
        CComPtr<IDCompositionRectangleClip> clip;
        if (FAILED(resources->composition->CreateVisual(&line))
                || FAILED(resources->composition->CreateRectangleClip(&clip))) return false;
        // Four moving strips expose the fixed dotted pattern. Neither the stroke
        // thickness nor the dots stretch as the rectangle grows.
        if (FAILED(edge == 3 ? clip->SetLeft(innerWidth) : clip->SetLeft(0.0f))
                || FAILED(edge == 1 ? clip->SetTop(innerHeight) : clip->SetTop(0.0f))) return false;
        if (edge == 2) {
            if (FAILED(clip->SetRight(1.0f))) return false;
        } else if (FAILED(clip->SetRight(width))) return false;
        if (edge == 0) {
            if (FAILED(clip->SetBottom(1.0f))) return false;
        } else if (FAILED(clip->SetBottom(height))) return false;
        return SUCCEEDED(line->SetContent(pattern)) && SUCCEEDED(line->SetClip(clip))
            && SUCCEEDED(visual->AddVisual(line, FALSE, nullptr));
    }

    bool begin(HWND destination, const RECT &origin, int showCommand) {
        resources = std::atomic_load(&graphics);
        if (!resources || !animationsEnabled() || origin.right <= origin.left || origin.bottom <= origin.top)
            return false;
        BOOL valid = FALSE;
        if (FAILED(resources->health->CheckDeviceState(&valid)) || !valid) return false;
        WNDCLASSW cls = {};
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpfnWndProc = windowProc;
        cls.lpszClassName = animationClass;
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        window = destination;
        identity = GetWindowLongPtrW(window, GWLP_USERDATA);
        foreground = GetForegroundWindow();
        const BOOL enabled = TRUE;
        if (FAILED(DwmSetWindowAttribute(window, DWMWA_CLOAK, &enabled, sizeof(enabled)))) {
            window = nullptr;
            return false;
        }
        DwmSetWindowAttribute(window, DWMWA_TRANSITIONS_FORCEDISABLED, &enabled, sizeof(enabled));
        ShowWindow(window, showCommand);
        if (!current()) return false;
        RECT end = {};
        if (FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &end, sizeof(end)))
                || end.right <= end.left || end.bottom <= end.top) {
            if (!GetWindowRect(window, &end)) return false;
        } else {
            // DWM/MSAA use physical screen pixels; this application is system-DPI-aware.
            POINT topLeft = {end.left, end.top}, bottomRight = {end.right, end.bottom};
            if (!PhysicalToLogicalPointForPerMonitorDPI(window, &topLeft)
                    || !PhysicalToLogicalPointForPerMonitorDPI(window, &bottomRight)) return false;
            end = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
        }
        RECT bounds = {};
        if (!UnionRect(&bounds, &origin, &end)) return false;
        overlay.reset(CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT
                | WS_EX_NOREDIRECTIONBITMAP, animationClass, L"", WS_POPUP,
            bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, cls.hInstance, this));
        if (!overlay || FAILED(resources->composition->CreateTargetForHwnd(overlay.get(), TRUE, &target))
                || FAILED(resources->composition->CreateVisual(&visual))
                || !drawPattern(static_cast<UINT>((std::max)(origin.right - origin.left, end.right - end.left)),
                    static_cast<UINT>((std::max)(origin.bottom - origin.top, end.bottom - end.top)))
                || FAILED(target->SetRoot(visual))) return false;
        LARGE_INTEGER start = {};
        if (!QueryPerformanceCounter(&start)) return false;
        CComPtr<IDCompositionAnimation> x, y, width, height, innerWidth, innerHeight;
        if (!animate(&x, static_cast<float>(origin.left - bounds.left), static_cast<float>(end.left - bounds.left), start)
                || !animate(&y, static_cast<float>(origin.top - bounds.top), static_cast<float>(end.top - bounds.top), start)
                || !animate(&width, static_cast<float>(origin.right - origin.left), static_cast<float>(end.right - end.left), start)
                || !animate(&height, static_cast<float>(origin.bottom - origin.top), static_cast<float>(end.bottom - end.top), start)
                || !animate(&innerWidth, static_cast<float>(origin.right - origin.left - 1), static_cast<float>(end.right - end.left - 1), start)
                || !animate(&innerHeight, static_cast<float>(origin.bottom - origin.top - 1), static_cast<float>(end.bottom - end.top - 1), start)
                || FAILED(visual->SetOffsetX(x)) || FAILED(visual->SetOffsetY(y))
                || !addEdge(width, height, innerWidth, innerHeight, 0)
                || !addEdge(width, height, innerWidth, innerHeight, 1)
                || !addEdge(width, height, innerWidth, innerHeight, 2)
                || !addEdge(width, height, innerWidth, innerHeight, 3)
                || !SetTimer(overlay.get(), 1, FolderOpeningAnimation::durationMs, nullptr)) return false;
        if (!SetWindowPos(overlay.get(), HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW)) return false;
        // One submission; DWM drives every intermediate frame. The timer only ends the effect.
        return SUCCEEDED(resources->composition->Commit());
    }
};

FolderOpeningAnimation::FolderOpeningAnimation() : state(new State) {}
FolderOpeningAnimation::~FolderOpeningAnimation() = default;

bool FolderOpeningAnimation::begin(HWND window, const RECT &origin, int showCommand) {
    return state->begin(window, origin, showCommand);
}

void FolderOpeningAnimation::prepare() {
    if (!animationsEnabled() || preparing.exchange(true)) return;
    lockProcess();
    if (!QueueUserWorkItem(prepareGraphics, nullptr, WT_EXECUTEDEFAULT)) {
        preparing.store(false);
        unlockProcess();
    }
}

void FolderOpeningAnimation::uninit() {
    // The worker holds a process lifetime reference until publication is complete.
    std::atomic_store(&graphics, std::shared_ptr<OpeningGraphics>());
}

} // namespace filespacer
