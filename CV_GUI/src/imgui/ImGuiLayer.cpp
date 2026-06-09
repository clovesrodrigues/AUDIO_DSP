#include "CV_GUI/imgui/ImGuiLayer.hpp"

#include "imgui.h"

namespace CV::GUI::ImGuiSupport {

ImGuiLayer::~ImGuiLayer ()
{
    destroy ();
}

bool ImGuiLayer::create ()
{
    if (context_)
        return true;

    IMGUI_CHECKVERSION ();
    context_ = ImGui::CreateContext ();
    if (!context_)
        return false;

    setCurrent ();
    ImGuiIO& io = ImGui::GetIO ();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark ();
    return true;
}

void ImGuiLayer::destroy ()
{
    if (!context_)
        return;

    setCurrent ();
    ImGui::DestroyContext (context_);
    context_ = nullptr;
}

void ImGuiLayer::setCurrent () const
{
    if (context_)
        ImGui::SetCurrentContext (context_);
}

void ImGuiLayer::beginFrame ()
{
    setCurrent ();
    ImGui::NewFrame ();
}

void ImGuiLayer::drawDefaultView ()
{
    setCurrent ();
    ImGui::Begin ("CV_GUI");
    ImGui::TextUnformatted ("CV_GUI Dear ImGui + VST3 infrastructure");
    ImGui::TextUnformatted ("Stages 5/6: ImGui frame and timer-driven render loop.");
    ImGui::End ();
}

ImDrawData* ImGuiLayer::render ()
{
    setCurrent ();
    ImGui::Render ();
    return ImGui::GetDrawData ();
}

void ImGuiLayer::addFocusEvent (bool focused)
{
    setCurrent ();
    ImGui::GetIO ().AddFocusEvent (focused);
}

bool ImGuiLayer::wantsMouseCapture () const
{
    setCurrent ();
    return ImGui::GetIO ().WantCaptureMouse;
}

bool ImGuiLayer::wantsKeyboardCapture () const
{
    setCurrent ();
    return ImGui::GetIO ().WantCaptureKeyboard;
}

bool ImGuiLayer::wantsTextInput () const
{
    setCurrent ();
    return ImGui::GetIO ().WantTextInput;
}

} // namespace CV::GUI::ImGuiSupport
