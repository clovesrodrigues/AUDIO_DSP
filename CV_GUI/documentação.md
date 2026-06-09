# CV_GUI — Manual de uso e programação

## 1. Objetivo do módulo

`CV_GUI` é a infraestrutura reutilizável de GUI do projeto `AUDIO_DSP`. O objetivo atual é permitir que plugins VST3 criem um editor baseado em **Dear ImGui** quando existir backend de plataforma disponível, mantendo sempre a possibilidade de fallback para a GUI nativa/genérica da DAW.

A arquitetura atual é:

```text
Plugin VST3 Controller
  -> CV::GUI::ImGuiBackend ou CV::GUI::HostFallbackBackend
    -> Steinberg::IPlugView
      -> CV::GUI::VST3ImGuiView
        -> Win32 child HWND
          -> OpenGL3/WGL context
            -> Dear ImGui
```

No estado atual, o backend gráfico implementado é **Win32 + OpenGL3**. Em plataformas sem esse backend, o caminho correto é retornar `nullptr` em `createView()` e deixar a DAW abrir a interface nativa de parâmetros.

---

## 2. Componentes principais

## 2.1 `CV::GUI::IGUIBackend`

Arquivo:

```text
CV_GUI/include/CV_GUI/IGUIBackend.hpp
```

### Função

Interface mínima para qualquer backend de GUI. Ela tem uma única responsabilidade: receber o nome da view VST3 e retornar um `Steinberg::IPlugView*` ou `nullptr`.

### API

```cpp
class IGUIBackend
{
public:
    virtual ~IGUIBackend () = default;
    virtual Steinberg::IPlugView* createView (Steinberg::FIDString name) = 0;
};
```

### Como usar

Use quando o plugin quiser abstrair se a view será ImGui, VSTGUI futura ou fallback nativo:

```cpp
std::unique_ptr<CV::GUI::IGUIBackend> backend;

#if COMPRESSOR_VST3_ENABLE_CV_GUI
backend = std::make_unique<CV::GUI::ImGuiBackend> (this);
#else
backend = std::make_unique<CV::GUI::HostFallbackBackend> ();
#endif

return backend->createView (name);
```

### O que não fazer

- Não colocar processamento de áudio nesta interface.
- Não armazenar ponteiros de DSP/audio thread dentro do backend.
- Não assumir que `createView()` sempre retornará uma view. `nullptr` é um resultado válido e desejado para fallback nativo.

---

## 2.2 `CV::GUI::HostFallbackBackend`

Arquivos:

```text
CV_GUI/include/CV_GUI/HostFallbackBackend.hpp
CV_GUI/src/HostFallbackBackend.cpp
```

### Função

Backend explícito de fallback. Ele retorna `nullptr`, permitindo que o host/DAW mostre a interface genérica de parâmetros VST3.

### Uso recomendado

Use quando:

- o backend de plataforma não existe;
- a build não é Windows;
- OpenGL/Win32 falhou;
- o usuário quer desabilitar GUI custom;
- você quer um modo seguro para depuração.

```cpp
CV::GUI::HostFallbackBackend fallback;
return fallback.createView (name); // retorna nullptr
```

### Métrica de sucesso

O plugin deve continuar carregando na DAW mesmo sem GUI customizada.

---

## 2.3 `CV::GUI::ImGuiBackend`

Arquivos:

```text
CV_GUI/include/CV_GUI/ImGuiBackend.hpp
CV_GUI/src/ImGuiBackend.cpp
```

### Função

Cria uma `VST3ImGuiView` quando a view pedida pelo host é `Steinberg::Vst::ViewType::kEditor`.

### API principal

```cpp
explicit ImGuiBackend (Steinberg::Vst::EditController* controller = nullptr);
ImGuiBackend (const Steinberg::ViewRect& initialSize,
              Steinberg::Vst::EditController* controller = nullptr);
Steinberg::IPlugView* createView (Steinberg::FIDString name) override;
```

### Exemplo mínimo no controller VST3

```cpp
Steinberg::IPlugView* PLUGIN_API MyController::createView (Steinberg::FIDString name)
{
#if MY_PLUGIN_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr; // fallback nativo da DAW
}
```

### Exemplo com tamanho inicial customizado

```cpp
Steinberg::ViewRect editorSize (0, 0, 1024, 640);
CV::GUI::ImGuiBackend guiBackend (editorSize, this);
return guiBackend.createView (name);
```

### O que não fazer

- Não criar `ImGuiBackend` global compartilhado entre instâncias do plugin.
- Não assumir que a view será criada em todas as plataformas.
- Não ignorar o `name`: a view só deve ser criada para `Vst::ViewType::kEditor`.

---

## 2.4 `CV::GUI::VST3ImGuiView`

Arquivos:

```text
CV_GUI/include/CV_GUI/VST3ImGuiView.hpp
CV_GUI/src/VST3ImGuiView.cpp
```

### Função

Implementação VST3 da view ImGui. Ela herda de `Steinberg::CPluginView` e gerencia:

- suporte de plataforma;
- `attached()` / `removed()`;
- foco;
- resize;
- criação/destruição do child window;
- criação/destruição do contexto OpenGL;
- inicialização/desligamento do ImGui;
- render loop por timer;
- encaminhamento de input Win32 para ImGui;
- ponte de automação VST3 via `VST3ParameterBridge`.

### Ciclo de vida

```text
createView()
  -> new VST3ImGuiView
    -> host chama attached(parent, type)
      -> cria child HWND
      -> cria OpenGL3 context
      -> cria ImGui context
      -> inicializa imgui_impl_win32
      -> inicializa imgui_impl_opengl3
      -> inicia WM_TIMER
      -> renderFrame()

host fecha editor
  -> removed()
    -> para timer
    -> shutdown ImGui renderer
    -> shutdown Win32 backend
    -> destroy ImGui context
    -> destroy OpenGL context
    -> destroy child HWND
```

### Métricas e especificações atuais

| Item | Valor atual |
|---|---:|
| Plataforma implementada | Win32 |
| Renderer implementado | OpenGL3/WGL |
| Intervalo do render loop | 33 ms, aproximadamente 30 FPS |
| Resize | Fixo, `canResize()` retorna `kResultFalse` |
| Fallback | `nullptr` no controller/plugin |
| Contexto ImGui | Um contexto por view/editor |
| Automação | Via `VST3ParameterBridge` |

### Input

A view trata:

- `onKeyDown()`;
- `onKeyUp()`;
- `onWheel()`;
- `WM_GETDLGCODE`;
- mensagens Win32 de teclado;
- mensagens Win32 de mouse;
- `WantCaptureKeyboard`;
- `WantCaptureMouse`;
- `WantTextInput`.

### O que não fazer

- Não renderizar ImGui fora da thread de UI.
- Não chamar funções ImGui sem garantir que o contexto correto está ativo.
- Não destruir OpenGL antes de desligar os backends ImGui.
- Não acessar áudio/DSP diretamente dentro da view.
- Não capturar atalhos da DAW quando ImGui não precisa capturar teclado.
- Não criar janelas top-level independentes dentro do plugin sem necessidade. Use child window anexada ao `HWND` do host.

---

## 2.5 `CV::GUI::ImGuiSupport::ImGuiLayer`

Arquivos:

```text
CV_GUI/include/CV_GUI/imgui/ImGuiLayer.hpp
CV_GUI/src/imgui/ImGuiLayer.cpp
```

### Função

Wrapper de contexto Dear ImGui. Ele garante que cada editor possa ter seu próprio `ImGuiContext`.

### Responsabilidades

- `ImGui::CreateContext()`;
- `ImGui::DestroyContext()`;
- `ImGui::SetCurrentContext()`;
- `ImGui::NewFrame()`;
- desenho inicial de infraestrutura;
- `ImGui::Render()`;
- consulta de captura de mouse/teclado/texto;
- envio de evento de foco ao ImGui.

### Exemplo conceitual

```cpp
ImGuiLayer layer;
if (!layer.create())
    return false;

layer.setCurrent();
layer.beginFrame();
layer.drawDefaultView();
ImDrawData* drawData = layer.render();
```

### Métrica de uso

Cada instância de editor deve ter seu próprio `ImGuiLayer`. Isso evita conflito entre múltiplas instâncias do mesmo plugin na DAW.

### O que não fazer

- Não usar um `ImGuiContext` global para todas as instâncias.
- Não chamar `ImGui::NewFrame()` duas vezes para o mesmo frame.
- Não destruir o contexto antes de desligar `imgui_impl_win32` e `imgui_impl_opengl3`.

---

## 2.6 `CV::GUI::Platform::Win32::Win32ChildWindow`

Arquivos:

```text
CV_GUI/include/CV_GUI/platform/win32/Win32ChildWindow.hpp
CV_GUI/src/platform/win32/Win32ChildWindow.cpp
```

### Função

Cria e gerencia a janela filha Win32 que será anexada ao `HWND` fornecido pelo host VST3.

### Responsabilidades

- registrar classe de janela Win32;
- criar child `HWND`;
- destruir child `HWND`;
- redimensionar child window;
- tratar `WM_GETDLGCODE`;
- expor callback de mensagens;
- expor callback de timer;
- iniciar/parar timer de render.

### Exemplo conceitual

```cpp
Win32ChildWindow child;
if (!child.create(parentHwnd, 800, 480))
    return false;

child.setMessageCallback(&MyMessageCallback, this);
child.startRenderTimer(33, &MyRenderCallback, this);
```

### O que não fazer

- Não criar janela top-level (`WS_OVERLAPPEDWINDOW`) para editor VST3 embutido.
- Não alterar o `HWND` pai da DAW além de anexar a child window.
- Não deixar timer ativo após `removed()`.
- Não usar `DestroyWindow()` depois que os recursos ImGui/OpenGL ainda precisam do HWND.

---

## 2.7 `CV::GUI::Renderer::OpenGL3::OpenGL3Context`

Arquivos:

```text
CV_GUI/include/CV_GUI/renderer/opengl3/OpenGL3Context.hpp
CV_GUI/src/renderer/opengl3/OpenGL3Context.cpp
```

### Função

Gerencia o contexto OpenGL/WGL usado pelo renderer ImGui.

### Responsabilidades

- obter `HDC`;
- configurar `PIXELFORMATDESCRIPTOR`;
- chamar `ChoosePixelFormat()`;
- chamar `SetPixelFormat()`;
- criar contexto via `wglCreateContext()`;
- tornar o contexto atual;
- limpar o framebuffer;
- executar `SwapBuffers()`;
- destruir o contexto.

### Especificação atual

| Item | Valor |
|---|---:|
| Color bits | 32 |
| Depth bits | 24 |
| Stencil bits | 8 |
| Double buffer | Sim |
| Contexto moderno via `wglCreateContextAttribsARB` | Ainda não implementado |

### O que precisa melhorar no futuro

- Adicionar tentativa de contexto moderno com `wglCreateContextAttribsARB`.
- Adicionar fallback explícito se OpenGL não estiver disponível.
- Adicionar logging de falhas de pixel format/context.
- Adicionar DPI/framebuffer scale.

---

## 2.8 `CV::GUI::Renderer::OpenGL3::ImGuiOpenGL3Renderer`

Arquivos:

```text
CV_GUI/include/CV_GUI/renderer/opengl3/ImGuiOpenGL3Renderer.hpp
CV_GUI/src/renderer/opengl3/ImGuiOpenGL3Renderer.cpp
```

### Função

Wrapper do backend oficial `imgui_impl_opengl3`.

### Responsabilidades

- `ImGui_ImplOpenGL3_Init()`;
- `ImGui_ImplOpenGL3_Shutdown()`;
- `ImGui_ImplOpenGL3_NewFrame()`;
- `ImGui_ImplOpenGL3_RenderDrawData()`.

### O que não fazer

- Não chamar `RenderDrawData()` sem contexto OpenGL atual.
- Não inicializar o renderer antes de criar contexto OpenGL.
- Não desligar OpenGL antes de `ImGui_ImplOpenGL3_Shutdown()`.

---

## 2.9 `CV::GUI::VST3::VST3ParameterBridge`

Arquivos:

```text
CV_GUI/include/CV_GUI/vst3/VST3ParameterBridge.hpp
CV_GUI/src/vst3/VST3ParameterBridge.cpp
```

### Função

Ponte entre widgets ImGui e automação VST3.

### Responsabilidades

- ler parâmetro normalizado;
- setar parâmetro normalizado;
- chamar `beginEdit()`;
- chamar `performEdit()`;
- chamar `endEdit()`;
- gerenciar gesture ativa.

### Uso recomendado para slider/knob ImGui

```cpp
Steinberg::Vst::ParamValue value = 0.0;
bridge.getParamNormalized(kParamGain, value);

float uiValue = static_cast<float>(value);
if (ImGui::SliderFloat("Gain", &uiValue, 0.0f, 1.0f))
{
    bridge.updateGesture(kParamGain, uiValue);
}

if (ImGui::IsItemDeactivatedAfterEdit())
{
    bridge.endGesture(kParamGain);
}
```

### Uso manual correto

```cpp
bridge.beginEdit(paramID);
bridge.performEdit(paramID, normalizedValue);
bridge.endEdit(paramID);
```

### O que não fazer

- Não chamar `performEdit()` sem `beginEdit()` para gestos interativos.
- Não esquecer `endEdit()` ao fim do drag/click.
- Não chamar automação a cada frame se o valor não mudou.
- Não acessar parâmetros VST3 diretamente da audio thread via GUI.
- Não usar valores fora do intervalo normalizado `[0.0, 1.0]`.

---

# 3. Integração em plugins VST3

## 3.1 Integração mínima no controller

```cpp
#include "CV_GUI/ImGuiBackend.hpp"

Steinberg::IPlugView* PLUGIN_API MyController::createView (Steinberg::FIDString name)
{
#if MY_PLUGIN_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}
```

## 3.2 Integração com CMake

```cmake
option(MY_PLUGIN_ENABLE_CV_GUI "Enable CV_GUI editor" ${WIN32})

if(MY_PLUGIN_ENABLE_CV_GUI AND WIN32)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../CV_GUI ${PROJECT_BINARY_DIR}/CV_GUI)
    target_link_libraries(my_plugin PRIVATE CV_GUI::cv_gui)
    target_compile_definitions(my_plugin PRIVATE MY_PLUGIN_ENABLE_CV_GUI=1)
else()
    target_compile_definitions(my_plugin PRIVATE MY_PLUGIN_ENABLE_CV_GUI=0)
endif()
```

## 3.3 Fallback nativo obrigatório

Sempre mantenha:

```cpp
return nullptr;
```

como fallback final de `createView()`.

Isso garante que:

- o plugin carrega em Linux/macOS enquanto o backend atual é Win32;
- a DAW ainda mostra parâmetros;
- falhas de GUI custom não impedem o uso do plugin.

---

# 4. Problemas conhecidos

## 4.1 Backend atual é Win32/OpenGL3

O backend gráfico completo ainda só existe para Windows. Em Linux, o build pode compilar a biblioteca, mas a view ImGui não será criada porque `VST3ImGuiView::supportsPlatformType()` só aceita `HWND` quando `SMTG_OS_WINDOWS` está ativo.

## 4.2 MinGW64 precisa validação real

O projeto foi pensado para MinGW64, mas o ambiente atual de teste não possui toolchain MinGW64 instalado. Portanto, ainda é necessário validar:

- headers Win32;
- link com `opengl32`, `gdi32`, `user32`;
- comportamento WGL;
- carregamento na DAW/Reaper em Windows.

## 4.3 OpenGL moderno ainda não foi implementado

A infraestrutura usa `wglCreateContext()`. Isso é suficiente para uma primeira validação, mas o ideal no futuro é tentar `wglCreateContextAttribsARB`.

## 4.4 Render loop fixo em timer

O render loop atual é por timer de aproximadamente 30 FPS. Para medidores e analisadores pode ser necessário:

- 60 FPS opcional;
- render sob demanda;
- pausa quando editor não está visível;
- controle de consumo de CPU.

## 4.5 Captura de teclado pode conflitar com DAW

Atalhos como espaço, Tab, setas, Ctrl+S, Ctrl+Z e Esc podem pertencer à DAW. O código deve respeitar `WantCaptureKeyboard` e não capturar teclado sem necessidade.

---

# 5. Boas práticas

## Faça

- Use `ImGuiBackend` no `createView()` do controller.
- Passe `this` quando o controller herda de `Steinberg::Vst::EditController`.
- Preserve fallback `nullptr`.
- Use `VST3ParameterBridge` para automação.
- Use valores normalizados para parâmetros VST3.
- Mantenha UI e áudio separados.
- Crie um contexto ImGui por editor.
- Faça shutdown na ordem correta.

## Não faça

- Não crie DSP dentro de `CV_GUI`.
- Não crie plugins dentro de `CV_GUI`.
- Não acesse audio buffers diretamente da GUI.
- Não bloqueie a UI thread com processamento pesado.
- Não use janelas top-level para editor embutido.
- Não ignore o fallback nativo da DAW.
- Não compartilhe `ImGuiContext` entre instâncias.
- Não chame automação fora do ciclo `beginEdit`/`performEdit`/`endEdit`.

---

# 6. Checklist antes de usar em produção

- [ ] Build em MinGW64 real.
- [ ] Build em MSVC.
- [ ] Teste no REAPER Windows.
- [ ] Teste abrir/fechar editor repetidamente.
- [ ] Teste múltiplas instâncias do plugin.
- [ ] Teste automação de parâmetros.
- [ ] Teste foco de teclado.
- [ ] Teste campos de texto ImGui.
- [ ] Teste resize se for habilitado futuramente.
- [ ] Teste unload/reload do plugin.
- [ ] Teste fallback nativo com `CV_GUI` desativado.

---

# 7. Próximas melhorias recomendadas

1. Adicionar widgets ImGui reais para parâmetros VST3.
2. Adicionar callback custom de desenho ao `ImGuiLayer`, em vez de apenas `drawDefaultView()`.
3. Implementar `wglCreateContextAttribsARB`.
4. Adicionar DPI/content scale.
5. Adicionar logs de falha para Win32/OpenGL.
6. Adicionar opção de FPS configurável.
7. Adicionar testes de build em MinGW64 e MSVC.
8. Adicionar backend Linux X11/Wayland somente se necessário.

