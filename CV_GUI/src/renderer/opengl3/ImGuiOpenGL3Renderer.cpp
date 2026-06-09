#include "CV_GUI/renderer/opengl3/ImGuiOpenGL3Renderer.hpp"

#include "backends/imgui_impl_opengl3.h"

namespace CV::GUI::Renderer::OpenGL3 {

ImGuiOpenGL3Renderer::~ImGuiOpenGL3Renderer ()
{
    destroy ();
}

bool ImGuiOpenGL3Renderer::create (const char* glslVersion)
{
    if (initialized_)
        return true;

    initialized_ = ImGui_ImplOpenGL3_Init (glslVersion);
    return initialized_;
}

void ImGuiOpenGL3Renderer::destroy ()
{
    if (!initialized_)
        return;

    ImGui_ImplOpenGL3_Shutdown ();
    initialized_ = false;
}

void ImGuiOpenGL3Renderer::newFrame ()
{
    if (initialized_)
        ImGui_ImplOpenGL3_NewFrame ();
}

void ImGuiOpenGL3Renderer::renderDrawData (ImDrawData* drawData)
{
    if (initialized_)
        ImGui_ImplOpenGL3_RenderDrawData (drawData);
}

} // namespace CV::GUI::Renderer::OpenGL3
