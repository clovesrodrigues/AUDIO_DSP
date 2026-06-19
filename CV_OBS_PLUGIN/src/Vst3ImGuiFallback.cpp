// Vst3ImGuiFallback.cpp — Editor Dear ImGui para plugins VST3 sem GUI nativa.
// Executa em thread separada com contexto OpenGL próprio (Win32+WGL ou X11+GLX).
// Documentação e comentários em PT-BR conforme preferências do projeto.

#include "CV_OBS_PLUGIN/Vst3ImGuiFallback.hpp"
#include "CV_OBS_PLUGIN/Vst3ParameterInfo.hpp"

#include <obs-module.h>

#include "pluginterfaces/vst/ivsteditcontroller.h"

// ImGui core
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

// ─── Includes específicos de plataforma ──────────────────────────────────────

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#  include <GL/gl.h>
#  include "backends/imgui_impl_win32.h"
   extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#elif defined(__linux__)
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/Xatom.h>
#  include <GL/gl.h>
#  include <GL/glx.h>
#endif

namespace cv_obs_plugin {
namespace {

// ─── Painel de controle de parâmetros (independente de plataforma) ───────────

void renderParameterPanel(const std::string&                       pluginName,
                           const std::vector<CachedParameterInfo>&  params,
                           Steinberg::Vst::IEditController*         ctrl,
                           float                                    displayW,
                           float                                    displayH) {
    ImGui::SetNextWindowPos ({0.f, 0.f},     ImGuiCond_Always);
    ImGui::SetNextWindowSize({displayW, displayH}, ImGuiCond_Always);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##params", nullptr, kFlags);

    ImGui::PushFont(nullptr);
    ImGui::TextUnformatted(pluginName.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    for (const auto& param : params) {
        using PF = Steinberg::Vst::ParameterInfo;
        if (param.flags & (PF::kIsReadOnly | PF::kIsHidden)) continue;

        const double currentNorm = ctrl->getParamNormalized(param.id);

        // Obtém string de display do próprio plugin
        Steinberg::Vst::String128 dispBuf = {};
        std::string               dispStr;
        if (ctrl->getParamStringByValue(param.id, currentNorm, dispBuf) == Steinberg::kResultOk) {
            for (const auto* p = dispBuf; *p; ++p) {
                const auto c = static_cast<unsigned>(static_cast<char16_t>(*p));
                if (c < 0x80) dispStr.push_back(static_cast<char>(c));
            }
        }

        const std::string label = param.title.empty()
            ? ("##p" + std::to_string(param.id))
            : (param.title + (param.units.empty() ? "" : " (" + param.units + ")"));

        ImGui::PushID(static_cast<int>(param.id));

        if (param.stepCount == 0) {
            // Parâmetro contínuo — slider normalizado 0..1
            float f = static_cast<float>(currentNorm);
            const std::string fmt = dispStr.empty() ? "%.3f" : dispStr;
            if (ImGui::SliderFloat(label.c_str(), &f, 0.f, 1.f, fmt.c_str()))
                ctrl->setParamNormalized(param.id, static_cast<double>(f));

        } else if (param.stepCount == 1) {
            // Toggle (bypass, on/off)
            bool on = (currentNorm >= 0.5);
            if (ImGui::Checkbox(label.c_str(), &on))
                ctrl->setParamNormalized(param.id, on ? 1.0 : 0.0);

        } else {
            // Discreto — botões de incremento/decremento
            const int current = static_cast<int>(std::round(currentNorm * param.stepCount));
            int       next    = current;

            ImGui::TextUnformatted(label.c_str());
            ImGui::SameLine();
            if (ImGui::ArrowButton("##dec", ImGuiDir_Left)  && current > 0)
                next = current - 1;
            ImGui::SameLine();
            ImGui::Text("[%s]", dispStr.empty() ? std::to_string(current).c_str()
                                                : dispStr.c_str());
            ImGui::SameLine();
            if (ImGui::ArrowButton("##inc", ImGuiDir_Right) && current < param.stepCount)
                next = current + 1;

            if (next != current)
                ctrl->setParamNormalized(param.id,
                    static_cast<double>(next) / static_cast<double>(param.stepCount));
        }

        ImGui::PopID();
    }

    ImGui::PopFont();
    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Implementações de plataforma
// ═══════════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
// ─── Windows: Win32 + WGL + ImGui ────────────────────────────────────────────

static constexpr const wchar_t* kWndClass = L"AUDIO_DSP_VST3_ImGuiEditor";

static LRESULT CALLBACK editorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return 1L;
    if (msg == WM_DESTROY) { ::PostQuitMessage(0); return 0L; }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

void runEditorThread(Vst3ImGuiEditorState* state) {
    WNDCLASSEXW wc     = {};
    wc.cbSize          = sizeof(wc);
    wc.lpfnWndProc     = editorWndProc;
    wc.hInstance       = ::GetModuleHandleW(nullptr);
    wc.hCursor         = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground   = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName   = kWndClass;
    ::RegisterClassExW(&wc);   // não falha se já registrada

    constexpr int kW = 560, kH = 620;
    RECT wr = {0, 0, kW, kH};
    ::AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    const std::wstring title(state->pluginName.begin(), state->pluginName.end());
    HWND hwnd = ::CreateWindowExW(
        0, kWndClass, title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);

    if (!hwnd) { state->running.store(false, std::memory_order_release); return; }

    HDC hdc = ::GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    const int fmt  = ::ChoosePixelFormat(hdc, &pfd);
    ::SetPixelFormat(hdc, fmt, &pfd);
    HGLRC hglrc = ::wglCreateContext(hdc);
    ::wglMakeCurrent(hdc, hglrc);

    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (state->running.load(std::memory_order_acquire)) {
        MSG msg = {};
        while (::PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                state->running.store(false, std::memory_order_release);
                break;
            }
        }
        if (!state->running.load(std::memory_order_acquire)) break;

        RECT cr = {};
        ::GetClientRect(hwnd, &cr);
        const float w = static_cast<float>(cr.right);
        const float h = static_cast<float>(cr.bottom);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        renderParameterPanel(state->pluginName, state->params, state->controller, w, h);

        ImGui::Render();
        ::glViewport(0, 0, cr.right, cr.bottom);
        ::glClearColor(0.11f, 0.11f, 0.11f, 1.f);
        ::glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ::SwapBuffers(hdc);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(ctx);
    ::wglMakeCurrent(nullptr, nullptr);
    ::wglDeleteContext(hglrc);
    ::ReleaseDC(hwnd, hdc);
    ::DestroyWindow(hwnd);
}

// ─── Linux: X11 + GLX + ImGui ────────────────────────────────────────────────

#elif defined(__linux__)

void runEditorThread(Vst3ImGuiEditorState* state) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        blog(LOG_WARNING, "AUDIO_DSP VST3 ImGui: XOpenDisplay falhou");
        state->running.store(false, std::memory_order_release);
        return;
    }

    const int screen = DefaultScreen(dpy);

    static const int kGlxAttribs[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None
    };
    XVisualInfo* vi = glXChooseVisual(dpy, screen, const_cast<int*>(kGlxAttribs));
    if (!vi) {
        blog(LOG_WARNING, "AUDIO_DSP VST3 ImGui: glXChooseVisual falhou");
        XCloseDisplay(dpy);
        state->running.store(false, std::memory_order_release);
        return;
    }

    Colormap cmap = XCreateColormap(dpy, DefaultRootWindow(dpy), vi->visual, AllocNone);

    XSetWindowAttributes swa = {};
    swa.colormap   = cmap;
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | KeyPressMask | StructureNotifyMask;

    constexpr unsigned kW = 560u, kH = 620u;
    Window win = XCreateWindow(
        dpy, DefaultRootWindow(dpy),
        0, 0, kW, kH, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa);

    XStoreName(dpy, win, state->pluginName.c_str());

    Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDelete, 1);
    XMapRaised(dpy, win);

    GLXContext ctx = glXCreateContext(dpy, vi, nullptr, GL_TRUE);
    glXMakeCurrent(dpy, win, ctx);
    XFree(vi);

    IMGUI_CHECKVERSION();
    ImGuiContext* imctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(imctx);
    ImGui::StyleColorsDark();
    ImGui_ImplOpenGL3_Init("#version 130");

    ImGuiIO& io      = ImGui::GetIO();
    io.DisplaySize   = {static_cast<float>(kW), static_cast<float>(kH)};
    io.DeltaTime     = 1.f / 60.f;

    auto lastFrame = std::chrono::steady_clock::now();

    while (state->running.load(std::memory_order_acquire)) {
        // Processa eventos X11
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
            case ClientMessage:
                if (static_cast<Atom>(ev.xclient.data.l[0]) == wmDelete)
                    state->running.store(false, std::memory_order_release);
                break;
            case ConfigureNotify:
                io.DisplaySize = {
                    static_cast<float>(ev.xconfigure.width),
                    static_cast<float>(ev.xconfigure.height)
                };
                glViewport(0, 0, ev.xconfigure.width, ev.xconfigure.height);
                break;
            case ButtonPress:
                io.MousePos = {static_cast<float>(ev.xbutton.x),
                               static_cast<float>(ev.xbutton.y)};
                if (ev.xbutton.button >= 1 && ev.xbutton.button <= 3)
                    io.MouseDown[ev.xbutton.button - 1] = true;
                break;
            case ButtonRelease:
                if (ev.xbutton.button >= 1 && ev.xbutton.button <= 3)
                    io.MouseDown[ev.xbutton.button - 1] = false;
                break;
            case MotionNotify:
                io.MousePos = {static_cast<float>(ev.xmotion.x),
                               static_cast<float>(ev.xmotion.y)};
                break;
            default:
                break;
            }
        }

        if (!state->running.load(std::memory_order_acquire)) break;

        const auto now = std::chrono::steady_clock::now();
        io.DeltaTime = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame    = now;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        renderParameterPanel(state->pluginName, state->params, state->controller,
                             io.DisplaySize.x, io.DisplaySize.y);

        ImGui::Render();
        glClearColor(0.11f, 0.11f, 0.11f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glXSwapBuffers(dpy, win);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext(imctx);
    glXMakeCurrent(dpy, None, nullptr);
    glXDestroyContext(dpy, ctx);
    XDestroyWindow(dpy, win);
    XFreeColormap(dpy, cmap);
    XCloseDisplay(dpy);
}

#else
// ─── Plataforma não suportada ─────────────────────────────────────────────────
void runEditorThread(Vst3ImGuiEditorState* state) {
    blog(LOG_WARNING,
         "AUDIO_DSP VST3: editor ImGui fallback não suportado nesta plataforma");
    state->running.store(false, std::memory_order_release);
}
#endif

} // namespace anônimo

// ─── API pública ──────────────────────────────────────────────────────────────

Vst3ImGuiEditorState* openImGuiFallbackEditor(
        const char*                             pluginName,
        const std::vector<CachedParameterInfo>& params,
        Steinberg::Vst::IEditController*        controller) {

    if (!controller || params.empty()) {
        blog(LOG_DEBUG, "AUDIO_DSP VST3 ImGui: plugin sem controller ou sem parâmetros");
        return nullptr;
    }

    auto* state       = new Vst3ImGuiEditorState;
    state->pluginName = pluginName ? pluginName : "VST3 Plugin";
    state->params     = params;
    state->controller = controller;
    state->running.store(true, std::memory_order_release);
    state->thread     = std::thread(runEditorThread, state);

    blog(LOG_INFO, "AUDIO_DSP VST3: editor ImGui aberto para '%s'", state->pluginName.c_str());
    return state;
}

void closeImGuiFallbackEditor(Vst3ImGuiEditorState*& state) {
    if (!state) return;
    state->running.store(false, std::memory_order_release);
    if (state->thread.joinable())
        state->thread.join();
    blog(LOG_INFO, "AUDIO_DSP VST3: editor ImGui fechado");
    delete state;
    state = nullptr;
}

} // namespace cv_obs_plugin
