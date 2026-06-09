# ImGui Test VST3

Plugin de teste criado a partir de `examples/TEMPLATE` para analisar a infraestrutura GUI disponível e validar a parte compilável da integração:

```text
VST3 + Dear ImGui
```

## Objetivo

O editor do plugin constrói um frame Dear ImGui contendo:

- texto (`ImGui::TextUnformatted`)
- botão (`ImGui::Button`)
- slider (`ImGui::SliderFloat`)
- listbox (`ImGui::ListBox`)

O teste não implementa DSP e não processa áudio. Ele registra apenas parâmetros VST3 auxiliares para refletir o estado dos controles ImGui durante a validação.

## Análise do VSTGUIEditor

Arquivos analisados no VST3 SDK local:

- `backends/vst3sdk/public.sdk/vst/vstguieditor.h`
- `backends/vst3sdk/public.sdk/vst/vstguieditor.cpp`

Conclusão técnica:

- `VSTGUIEditor` é a classe do SDK para editores baseados em **VSTGUI**.
- Ela herda de `EditorView`, `VSTGUI::VSTGUIEditorInterface` e `VSTGUI::CBaseObject`.
- O header inclui `vstgui/vstgui.h`, mas o repositório contém apenas os wrappers `vstguieditor.*` e arquivos auxiliares `vstgui_*`; a biblioteca/header real `vstgui/vstgui.h` não está presente em `backends/vst3sdk`.
- Sem `vstgui/vstgui.h`, não é possível compilar uma classe derivada de `VSTGUIEditor` neste repositório sem adicionar dependência externa ou criar um mock de VSTGUI, ambos fora do escopo.

Por isso, o teste usa `Steinberg::Vst::EditorView` diretamente e cria um contexto Dear ImGui com o backend existente `backends/imgui/backends/imgui_impl_null.cpp`. Isso valida que o plugin VST3 consegue instanciar o editor e gerar frames Dear ImGui, mas **não fornece renderização nativa visível** porque o repositório também não possui um backend Dear ImGui para X11/Wayland/Win32/macOS acoplado ao `IPlugView` VST3.

## Arquivos utilizados na integração

### VST3

- `backends/vst3sdk/public.sdk/vst/vsteditcontroller.h`
- `backends/vst3sdk/public.sdk/vst/vsteditcontroller.cpp`
- `backends/vst3sdk/public.sdk/vst/vstaudioeffect.h`
- `backends/vst3sdk/public.sdk/vst/vstaudioeffect.cpp`
- `backends/vst3sdk/public.sdk/common/pluginview.h`
- `backends/vst3sdk/public.sdk/common/pluginview.cpp`
- `backends/vst3sdk/public.sdk/main/pluginfactory.h`
- `backends/vst3sdk/public.sdk/main/pluginfactory.cpp`

### VSTGUIEditor analisado

- `backends/vst3sdk/public.sdk/vst/vstguieditor.h`
- `backends/vst3sdk/public.sdk/vst/vstguieditor.cpp`

### Dear ImGui

- `backends/imgui/imgui.h`
- `backends/imgui/imgui.cpp`
- `backends/imgui/imgui_draw.cpp`
- `backends/imgui/imgui_tables.cpp`
- `backends/imgui/imgui_widgets.cpp`
- `backends/imgui/backends/imgui_impl_null.h`
- `backends/imgui/backends/imgui_impl_null.cpp`

### Plugin de teste

- `source/TPprocessor.*`: componente VST3 sem áudio/DSP.
- `source/TPcontroller.*`: registra parâmetros auxiliares e cria o editor.
- `source/TPimguieditor.*`: cria o contexto Dear ImGui e gera o frame com texto, botão, slider e listbox.
- `source/TPentry.cpp`: registra processor/controller no factory VST3.
- `source/TPcids.h`: FUIDs e IDs de parâmetros.
- `CMakeLists.txt`: build autônomo usando VST3 SDK e Dear ImGui locais.

## Parâmetros auxiliares

| Parâmetro | Origem no frame ImGui | Observação |
| --- | --- | --- |
| ImGui Slider | `ImGui::SliderFloat` | Reflete valor normalizado 0..1. |
| ImGui Listbox | `ImGui::ListBox` | Opções: Compressor, Limiter, Noise Gate. |
| Button Counter | `ImGui::Button` | Contador normalizado até 16 cliques. |

## Instruções de build

A partir da raiz do repositório:

```bash
cmake -S examples/imgui_test_vst3 -B build/imgui_test_vst3
cmake --build build/imgui_test_vst3
```

No Linux, o artefato VST3 é gerado em:

```text
build/imgui_test_vst3/VST3/imgui_test_vst3.vst3/Contents/x86_64-linux/imgui_test_vst3.so
```

## Limitações atuais

- `VSTGUIEditor` não é usado diretamente porque a dependência `vstgui/vstgui.h` não existe no repositório.
- O backend `imgui_impl_null` não desenha pixels em uma janela do host; ele valida criação de contexto e geração de draw data do Dear ImGui.
- Para uma janela visual real, seria necessário adicionar a biblioteca VSTGUI completa ou implementar um backend Dear ImGui de plataforma/renderização para o `IPlugView` do host. Isso não foi feito para respeitar a restrição de não baixar dependências externas e não criar frameworks novos.

## Recursos copiados do TEMPLATE

Os arquivos PNG de snapshot do `examples/TEMPLATE/resource` não são duplicados neste exemplo para evitar diffs binários no pull request. O teste de ImGui não referencia snapshots PNG no CMake; valida apenas a criação do editor e a geração do frame Dear ImGui. Se snapshots forem necessários futuramente, gere-os no build ou copie-os localmente a partir do template fora do controle de versão.
