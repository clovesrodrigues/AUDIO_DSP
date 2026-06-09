# GUI Manager Test VST3

## Objetivo

`gui_test_vst3` valida a infraestrutura `examples/gui_manager` sem implementar DSP. O plug-in contém apenas controles de GUI e parâmetros auxiliares:

- botão (`Button Counter`)
- slider (`Slider`)
- knob lógico (`Knob`)
- listbox (`Listbox`)

## Módulo GUI criado

O módulo separado fica em `examples/gui_manager` e não tem dependência do `CV_DSP`.

Classes/interfaces criadas:

| Classe | Responsabilidade |
| --- | --- |
| `CV::GUI::IGUIBackend` | Interface comum para backends de GUI. |
| `CV::GUI::ImGuiBackend` | Cria um `EditorView` VST3 e valida widgets Dear ImGui usando Dear ImGui local. |
| `CV::GUI::VSTGUIBackend` | Ponto de integração para VSTGUI; detecta a disponibilidade conceitual e cai para host nativo enquanto o runtime VSTGUI completo não é linkado neste exemplo leve. |
| `CV::GUI::NativeHostBackend` | Fallback oficial: retorna `nullptr` em `createView()` para que o host/DAW mostre a interface padrão de parâmetros VST3. |
| `CV::GUI::GUIManager` | Seleciona backend por preferência (`Auto`, `ImGui`, `VSTGUI`, `Native`) e fornece uma única chamada `createView()`. |

## Backends analisados

- `backends/imgui`: Dear ImGui e backends oficiais estão presentes. Não há backend pronto que conecte Dear ImGui diretamente ao `Steinberg::IPlugView`; por isso o teste usa o backend `imgui_impl_null` somente para validar lifecycle e geração de frame, não para renderização visível real.
- `backends/vst3sdk`: SDK VST3 local usado para `AudioEffect`, `EditControllerEx1`, `EditorView`, `IPlugView` e factory.
- `backends/vst3sdk/vstgui4`: contém `vstgui/vstgui.h` e fontes do VSTGUI4. O `VSTGUIBackend` foi criado como ponto de extensão, mas o exemplo não linka todo o runtime VSTGUI4 para evitar transformar a infraestrutura inicial em uma integração VSTGUI completa e frágil.

## Seleção de backend

Configure o backend preferido com `CV_GUI_TEST_BACKEND`:

```bash
cmake -S examples/gui_test_vst3 -B build/gui_test_vst3 -DCV_GUI_TEST_BACKEND=Auto
cmake --build build/gui_test_vst3
```

Valores aceitos:

| Valor | Resultado esperado |
| --- | --- |
| `Auto` | Tenta `ImGuiBackend`, depois `VSTGUIBackend`, depois `NativeHostBackend`. |
| `ImGui` | Seleciona `ImGuiBackend` quando Dear ImGui está disponível. |
| `VSTGUI` | Tenta `VSTGUIBackend`; neste momento cai para fallback se o runtime completo não estiver linkado. |
| `Native` | Usa `NativeHostBackend` e retorna `nullptr` para editor nativo do host. |

## Parâmetros VST3

| Parâmetro | Tipo |
| --- | --- |
| `Button Counter` | Range 0..16 |
| `Slider` | Range 0..1 |
| `Listbox` | String list: Dear ImGui, VSTGUI, Native Host |
| `Knob` | Range 0..1 |

## Build

```bash
cmake -S examples/gui_test_vst3 -B build/gui_test_vst3
cmake --build build/gui_test_vst3
```

No Linux, o VST3 é gerado em:

```text
build/gui_test_vst3/VST3/gui_test_vst3.vst3/Contents/x86_64-linux/gui_test_vst3.so
```

## Status atual

- Dear ImGui: compila e valida lifecycle/widgets via `ImGuiBackend` com `imgui_impl_null`.
- VSTGUI: camada `VSTGUIBackend` criada; fallback documentado até o exemplo linkar o runtime completo do VSTGUI4.
- Fallback DAW: implementado por `NativeHostBackend` retornando `nullptr`.

## Limitações conhecidas

- `imgui_impl_null` não renderiza pixels em uma janela real; ele existe aqui apenas para testar a abstração e o lifecycle do editor.
- `VSTGUIBackend` ainda não cria controles VSTGUI reais porque isso exige linkar e validar um conjunto maior de fontes/runtime do VSTGUI4.
- O exemplo não contém DSP por design.
