#pragma once

struct ImDrawData;
struct ImGuiContext;

namespace CV::GUI::ImGuiSupport {

class ImGuiLayer
{
public:
    ImGuiLayer () = default;
    ~ImGuiLayer ();

    ImGuiLayer (const ImGuiLayer&) = delete;
    ImGuiLayer& operator= (const ImGuiLayer&) = delete;

    bool create ();
    void destroy ();
    void setCurrent () const;
    bool isValid () const noexcept { return context_ != nullptr; }

    void beginFrame ();
    void drawDefaultView ();
    ImDrawData* render ();

    void addFocusEvent (bool focused);
    bool wantsMouseCapture () const;
    bool wantsKeyboardCapture () const;
    bool wantsTextInput () const;

private:
    ImGuiContext* context_ {nullptr};
};

} // namespace CV::GUI::ImGuiSupport
