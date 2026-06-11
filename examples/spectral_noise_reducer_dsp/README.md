# SpectralNoiseReducer DSP smoke example

Este exemplo valida o `cvdsp::spectral::SpectralNoiseReducer` legado e o novo
`cvdsp::spectral::RealtimeNoiseReducer` como DSPs de **percepcao/reducao de
ruido** para ficar no inicio da cadeia de gravacao. Ele nao e um pedal: a ideia
e aprender o ruido de fundo antes dos pedais, subtrair o perfil aprendido e
depois alimentar o restante do set com um sinal mais limpo.

## Controles essenciais

- **Perceber Ruido** (`setLearnNoiseEnabled`) liga/desliga a captura do perfil de
  ruido.
- **Subtrair Ruidos** (`setSubtractNoiseEnabled`) liga/desliga a aplicacao do
  perfil aprendido ao audio.
- **Limpar Perfil** (`triggerClearProfile`) continua disponivel no core para testes
  e integracoes avancadas, mas o VST3 minimalista nao expoe esse botao: iniciar
  **Perceber Ruido** novamente ja captura um perfil novo.

Fluxo recomendado para gravacao:

1. Deixe guitarra/microfone/cabo em silencio, mas com o ganho real da sessao.
2. Ligue **Perceber Ruido** por alguns segundos para capturar hum, hiss, cabo,
   aterramento e ruido ambiente estacionario.
3. Desligue **Perceber Ruido**.
4. Mantenha **Subtrair Ruidos** ligado durante a gravacao para aplicar a limpeza.
5. No VST3, ligue **Perceber Ruido** novamente quando trocar interface, captador,
   cabo, ganho ou sala, porque isso inicia uma captura nova.

> Se **Subtrair Ruidos** tambem for desligado, o DSP entra em bypass real. Isso e
> util para comparacao A/B, mas nao aplica reducao durante a gravacao.

## Controles publicos recomendados

O VST3 deve expor apenas `Perceber Ruido`, `Subtrair Ruido`, `Ganho`,
`Presenca` e `Smooth`. O smoke example tambem exercita o `RealtimeNoiseReducer`, que e o core limpo
usado pelo VST3 minimalista, incluindo um teste multi-instancia para garantir
que perfis e historicos de ganho nao vazam entre faixas.

## Build com CMake

A partir da raiz do repositorio:

```bash
cmake -S examples/spectral_noise_reducer_dsp -B build/examples/spectral_noise_reducer_dsp
cmake --build build/examples/spectral_noise_reducer_dsp
./build/examples/spectral_noise_reducer_dsp/spectral_noise_reducer_smoke
```

## Build direto com compilador

```bash
c++ -std=c++20 -Wall -Wextra -Wpedantic -I. \
    examples/spectral_noise_reducer_dsp/spectral_noise_reducer_smoke.cpp \
    -o /tmp/spectral_noise_reducer_smoke && \
    /tmp/spectral_noise_reducer_smoke
```

O programa aprende um ruido sintetico, ativa a subtracao, verifica que a energia
RMS cai, valida snapshots espectrais brutos, valida `GuiSnapshot` com curvas em
dB/normalizadas para a futura GUI e confirma o bypass quando os dois botoes
principais estao desligados.
