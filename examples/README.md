# AUDIO_DSP examples

Esta pasta concentra exemplos de uso da biblioteca `CV_DSP`, incluindo smoke
programs sem SDK de host, projetos VST3 de pedais e utilitários VST3 que não são
pedais.

## 1. Smoke examples de DSP puro

Os exemplos abaixo compilam com um compilador C++20 comum e não precisam de VST3,
CV_GUI ou DAW. Eles servem para validar inclusão de headers, `prepare/reset`,
processamento básico, ausência de `NaN/Inf` e descritores quando aplicável.

```bash
c++ -std=c++20 -Wall -Wextra -Wpedantic -I. examples/expression_engine_dsp/expression_engine_smoke.cpp -o /tmp/expression_engine_smoke && /tmp/expression_engine_smoke
c++ -std=c++20 -Wall -Wextra -Wpedantic -I. examples/phaser_dsp/phaser_smoke.cpp -o /tmp/phaser_smoke && /tmp/phaser_smoke
c++ -std=c++20 -Wall -Wextra -Wpedantic -I. examples/sustainer_dsp/sustainer_smoke.cpp -o /tmp/sustainer_smoke && /tmp/sustainer_smoke
c++ -std=c++20 -Wall -Wextra -Wpedantic -I. examples/wah_wah_dsp/wah_wah_smoke.cpp -o /tmp/wah_wah_smoke && /tmp/wah_wah_smoke
c++ -std=c++20 -Wall -Wextra -Wpedantic -I. examples/spectral_noise_reducer_dsp/spectral_noise_reducer_smoke.cpp -o /tmp/spectral_noise_reducer_smoke && /tmp/spectral_noise_reducer_smoke
c++ -std=c++20 -Wall -Wextra -Wpedantic -I. examples/guitar_pedalboard_dsp/guitar_pedalboard_smoke.cpp -o /tmp/guitar_pedalboard_smoke && /tmp/guitar_pedalboard_smoke
```

O redutor de ruido tambem possui um benchmark CMake leve para comparar custo de
CPU entre mudancas de codigo sem abrir a DAW:

```bash
cmake -S examples/spectral_noise_reducer_dsp -B /tmp/spectral_noise_reducer_dsp_build
cmake --build /tmp/spectral_noise_reducer_dsp_build --target realtime_noise_reducer_benchmark
/tmp/spectral_noise_reducer_dsp_build/realtime_noise_reducer_benchmark
```

Para validar todos os smoke examples novos e o benchmark spectral em um unico
comando:

```bash
examples/run_new_dsp_smokes.sh
```

## 2. Pedais VST3

Os pedais ficam em `examples/pedais/` e podem ser compilados em lote:

```bash
examples/pedais/build_all_pedals.sh
```

Por padrão, os builds são gerados fora do repositório em:

```text
/tmp/cv_dsp_pedais_vst3_build
```

Para escolher outra pasta:

```bash
examples/pedais/build_all_pedals.sh /tmp/minha_build_pedais
```

Veja o tutorial completo em `examples/pedais/README.md`.

## 3. SpectralNoiseReducer VST3 utility

`examples/spectral_noise_reducer_vst3` é propositalmente separado de
`examples/pedais`: ele não é pedal, e sim um utilitário de entrada para aprender
e subtrair ruído antes de montar a cadeia de pedais/efeitos.

Build individual:

```bash
cmake -S examples/spectral_noise_reducer_vst3 -B /tmp/spectral_noise_reducer_vst3_build
cmake --build /tmp/spectral_noise_reducer_vst3_build
```

Fluxo de uso esperado na DAW:

1. Inserir o VST3 primeiro na cadeia de gravação.
2. Deixar microfone/guitarra em silêncio com o ganho real.
3. Ligar **Perceber Ruído** para capturar o perfil.
4. Desligar **Perceber Ruído**.
5. Ligar **Subtrair Ruídos** e gravar.
6. Ligar **Perceber Ruído** novamente quando trocar cabo, captador, sala,
   interface ou ganho; isso limpa o perfil anterior e captura um perfil novo.

Checklist recomendado no REAPER depois de instalar uma build nova:

- Re-escanear o VST3 para garantir que a DAW carregou a DLL/SO mais recente.
- Conferir no painel de FX se o plugin informa **1024 spls** de latência/PDC.
- Duplicar uma faixa com o mesmo áudio e verificar se não aparece voz robótica
  por desalinhamento de fase entre faixa processada e rota paralela.
- Medir CPU com **Perceber Ruído** e **Subtrair Ruídos** desligados; nesse estado
  o VST3 usa pass-through de bloco.

## 4. GUI / fallback

Os projetos VST3 usam a estratégia de fallback adotada no repositório: se a GUI
CV_GUI não estiver habilitada, disponível ou funcional na plataforma, o
`createView()` retorna `nullptr` e a DAW pode abrir o editor nativo/genérico de
parâmetros. Assim, o DSP continua utilizável mesmo antes da GUI customizada estar
100% pronta.

## 5. Warnings conhecidos

Em Linux, o VST3 SDK bundled pode emitir warnings de formato (`%lld`/`%llu`) em
arquivos da Steinberg. Esses warnings são externos ao DSP e não impedem a geração
dos plug-ins.
