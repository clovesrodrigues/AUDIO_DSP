# CV Compressor VST3

Plugin VST3 de compressor estéreo criado a partir de `examples/TEMPLATE` sem alterar a pasta original do template.

## DSP utilizado

A categoria solicitada foi **Compressor**. A busca no repositório encontrou uma única implementação explícita de compressor:

- `cvdsp::dynamics::Compressor<float>` em `CV_DSP/Dynamics/Compressor.hpp`

Como existe apenas uma implementação compatível, o plugin usa somente essa classe e não expõe uma listbox de seleção de algoritmo.

## Parâmetros

| Parâmetro | Faixa | Padrão | Descrição |
| --- | ---: | ---: | --- |
| Threshold | -60 a 0 dB | -18 dB | Nível acima do qual a compressão atua. |
| Ratio | 1:1 a 20:1 | 4:1 | Intensidade da compressão. |
| Attack | 0.1 a 200 ms | 10 ms | Tempo de ataque do detector de envelope. |
| Release | 10 a 1000 ms | 100 ms | Tempo de retorno do detector. |
| Knee | 0 a 24 dB | 6 dB | Largura da transição soft-knee. 0 dB equivale a hard-knee. |
| Makeup | 0 a 24 dB | 3 dB | Ganho de compensação após a redução. |
| Detector | Peak / RMS | Peak | Modo do `EnvelopeFollower` usado pelo compressor. |

## Dependências

- VST3 SDK presente no próprio repositório em `backends/vst3sdk`.
- CV_DSP header-only presente no próprio repositório em `CV_DSP`.
- CMake 3.14 ou superior.
- Compilador C++20.

Nenhuma dependência externa é baixada.

## GUI

O plugin agora integra a infraestrutura `CV_GUI` de forma condicional. Em plataformas com backend disponível (`WIN32`, usando child window Win32 + OpenGL3), `COMPRESSOR_VST3_ENABLE_CV_GUI` pode criar um editor Dear ImGui via `IPlugView`. Em plataformas sem backend CV_GUI disponível ou quando a opção estiver desativada, `createView()` retorna `nullptr` e preserva o fallback nativo do host VST3: os parâmetros registrados no controller ficam disponíveis na interface genérica da DAW.

No Linux deste repositório, o fallback nativo é usado por padrão porque o backend CV_GUI implementado até agora é Win32/OpenGL3.

## Instruções de build

A partir da raiz do repositório:

```bash
cmake -S examples/compressor_vst3 -B build/compressor_vst3
cmake --build build/compressor_vst3
```

No Linux, o artefato VST3 é gerado em:

```text
build/compressor_vst3/VST3/compressor_vst3.vst3/Contents/x86_64-linux/compressor_vst3.so
```

## Observações

- A pasta `examples/TEMPLATE` não foi modificada.
- O processamento é estéreo, com uma instância `cvdsp::dynamics::Compressor<float>` por canal.
- Amostras VST3 `kSample32` são suportadas.
- Estado/presets salvam todos os parâmetros do compressor.

## Recursos copiados do TEMPLATE

Os arquivos PNG de snapshot do `examples/TEMPLATE/resource` não são duplicados neste exemplo para evitar diffs binários no pull request. O plugin usa UI nativa VST3/fallback do host e não referencia snapshots PNG no CMake. Se snapshots forem necessários futuramente, gere-os no build ou copie-os localmente a partir do template fora do controle de versão.
