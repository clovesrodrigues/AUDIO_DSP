#pragma once

struct ImDrawData;

namespace CV::GUI::Renderer::OpenGL3 {

class ImGuiOpenGL3Renderer
{
public:
    ImGuiOpenGL3Renderer () = default;
    ~ImGuiOpenGL3Renderer ();

    ImGuiOpenGL3Renderer (const ImGuiOpenGL3Renderer&) = delete;
    ImGuiOpenGL3Renderer& operator= (const ImGuiOpenGL3Renderer&) = delete;

    bool create (const char* glslVersion = nullptr);
    void destroy ();
    void newFrame ();
    void renderDrawData (ImDrawData* drawData);

    bool isValid () const noexcept { return initialized_; }

private:
    bool initialized_ {false};
};

} // namespace CV::GUI::Renderer::OpenGL3
