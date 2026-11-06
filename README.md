# X-Men 2: Clone Wars — Native Nintendo 64 Port

[Português do Brasil](#português-do-brasil) | [English](#english)

---

## Português do Brasil

Porte nativo de **X-Men 2: Clone Wars** (Sega Genesis / Mega Drive) para o **Nintendo 64**, baseado na engenharia reversa e metodologia de recompilação estática do projeto `crabsmack/xmen2-pc`.

O código da lógica original em Motorola 68000 foi traduzido para C/C++ estático e compilado nativamente para a arquitetura MIPS VR4300 do N64, eliminando a sobrecarga de interpretadores e emuladores. Gráficos e áudio são executados diretamente no hardware do Nintendo 64 (RDP e RSP).

### Características e Arquitetura

- **Variante de Referência (`heavy_60_opt`)**: Otimizada para entrega estável em 60 quadros por segundo (60 Hz / 1 VBlank no padrão NTSC).
- **Renderizador RDP Nativo**:
  - Extração de planos 2D em passes de prioridade unificados (`x2_plane_dual`).
  - Cache de comandos RDP para planos com scroll uniforme (`X2_PLANE_CMD_CACHE`), minimizando a carga da CPU.
  - Renderização direta de sprites pelo RDP com máscaras de textura CI4 e recorte nativo.
  - Suporte completo a efeitos de sombra e destaque (*Shadow/Highlight*) via materiais RDP de duas passagens.
  - Triplo buffer no display do N64 (`display_init` com 3 buffers de 16 bpp) para desacoplar a renderização da CPU.
- **Sistema de Áudio Nativo via RSP**:
  - Sequenciador de música GEMS reescrito nativamente em C++.
  - Síntese por reprodução de amostras PCM com 8 vozes via RSP mixer do *libdragon*.
  - Amostras pré-processadas com variantes de afinação/taxa (`voices_v.pcm`), reduzindo drasticamente o tráfego DMA do cartucho.
  - Processamento fatiado (*sliced mixing*) intercalado estrategicamente na janela de espera da apresentação de vídeo, evitando quedas de quadros (*hitches*).
- **Sem Telemetria**: Código limpo de diagnóstico e instrumentação, sem chamadas de profiling em tempo de execução, garantindo desempenho máximo.

---

### Estado Atual

- **Compatibilidade**: Executável no Nintendo 64 real (EverDrive-64, SummerCart64) e em emuladores com precisão de baixo nível (Ares v148+, simple64).
- **Taxa de Quadros**: 60 FPS reais na grande maioria dos cenários e combates intensos da variante `heavy_60_opt`.
- **Fidelidade Gráfica**: Planos A e B, prioridades de tiles, animações, operadores de transparência/sombra fiéis ao Mega Drive.
- **Fidelidade Sonora**: Músicas, efeitos sonoros e vozes digitalizadas reproduzidos com clareza a 12 kHz / 16-bit estéreo.

---

### Trabalhos Futuros e Melhorias em Andamento

1. **Tradução (PT-BR)**: Integração dos scripts e fontes da tradução para o português do Brasil.
2. **Sistema de Salvamento (Save)**: Implementação de salvamento persistente na memória do cartucho (SRAM / EEPROM / FlashRAM) para salvar o progresso entre as fases.
3. **Suporte a 4 Jogadores**: Auditoria e habilitação das rotinas de controle para 4 jogadores simultâneos (`game/runtime/players34.cpp`).

---

### Requisitos de Sistema e Ferramentas

Para compilar a ROM, você precisará de:

- **Sistema Operacional**:
  - Linux (Ubuntu 20.04/22.04/24.04 recomendados) **OU**
  - Windows 10/11 com **WSL2** (Windows Subsystem for Linux - Ubuntu).
- **Pacotes do Sistema (Host)**:
  - `build-essential` (`gcc`, `g++`, `make`)
  - `python3` (versão 3.8 ou superior)
- **libdragon Toolchain & SDK**:
  - SDK do libdragon instalado (caminho padrão `/opt/libdragon` ou definido na variável `N64_INST`).
  - Utilitários MIPS (`mips64-elf-gcc`, `mips64-elf-g++`, `mips64-elf-nm`, `mkdfs`, `n64tool`).
- **ROM Original do Jogo**:
  - É necessário fornecer um dump limpo e legítimo do cartucho original do Sega Genesis / Mega Drive.

---

### Requisitos da ROM de Entrada

Por motivos legais, este repositório **NÃO contém nenhuma ROM, asset gráfico ou dado de áudio proprietário**. Você deve fornecer seu próprio arquivo ROM extraído de seu cartucho original.

O arquivo deve ser colocado exatamente no caminho:
`game/xmen2.bin`

#### Especificações da ROM:
- **Título**: *X-Men 2: Clone Wars* (USA/Europe, Rev 00 / World)
- **Formato**: Binário puro (flat binary, descompactado, sem cabeçalhos SMD/BIN de emulador)
- **Tamanho exato**: 2.097.152 bytes (2 MiB)
- **Hashes de Verificação**:
  - **SHA-1**: `61409e6cf6065ab67d8952b891d8edcf47777193`
  - **SHA-256**: `30bb09be17b15c9b86683967459fe3b9162f4d77dafab466b39c1b86254ee58f`
  - **MD5**: `b687660446f6e49dfd465cd0ef9f0bc3`

---

### Instruções de Compilação

#### 1. Clonar o Repositório

Certifique-se de clonar com os submódulos (`--recursive`):

```bash
git clone --recursive https://github.com/Sunshinin11/X-men-2-clone-wars-64.git
cd X-men-2-clone-wars-64
```

Se já tiver clonado sem `--recursive`, inicialize o submódulo:
```bash
git submodule update --init --recursive
```

#### 2. Adicionar a ROM

Copie sua ROM de Mega Drive verificada para a pasta `game/`:
```bash
cp /caminho/para/sua/rom.bin game/xmen2.bin
```

#### 3. Compilar

##### No Linux / WSL:
Execute o script de compilação automatizado:
```bash
chmod +x build_n64.sh
./build_n64.sh
```

##### No Windows (PowerShell):
Se você utiliza Windows com WSL configurado:
```powershell
powershell -ExecutionPolicy Bypass -File .\build_n64.ps1
```

#### 4. Resultado da Compilação

Após a conclusão, a ROM do Nintendo 64 será gerada na raiz do projeto:
- **`xmen2_heavy_60_opt.z64`**

Você pode carregá-la diretamente no Ares v148+, simple64 ou transferi-la para seu EverDrive-64.

---

### Resolução de Problemas

- **Erro `missing game ROM` ou `short ROM`**:
  O arquivo `game/xmen2.bin` não foi encontrado ou não possui exatamente 2.097.152 bytes. Verifique o hash do seu dump.
- **Erro `command not found: mips64-elf-gcc` ou `N64_INST` ausente**:
  O SDK libdragon não está instalado ou configurado no PATH. Instale o libdragon em `/opt/libdragon` ou defina a variável `export N64_INST=/seu/caminho/libdragon`.
- **Erro de quebra de linha (`\r: command not found`)**:
  Se você clonou no Windows com conversão CRLF e executou no WSL, execute:
  ```bash
  dos2unix build_n64.sh
  ```
- **Erro `submodule segagenesisrecomp not found`**:
  Execute `git submodule update --init --recursive`.

---

### Licença e Direitos Autorais

- O código deste porte é disponibilizado sob os termos das licenças open-source do projeto base `crabsmack/xmen2-pc` e do SDK `libdragon`.
- *X-Men 2: Clone Wars* é marca registrada e propriedade intelectual da Sega Enterprises, Marvel Characters, Inc. e seus respectivos detentores de direitos.
- Este projeto não contém, não compartilha e não apoia a distribuição de material protegido por direitos autorais.

---

## English

Native port of **X-Men 2: Clone Wars** (Sega Genesis / Mega Drive) for the **Nintendo 64**, based on the reverse engineering and static recompilation methodology of `crabsmack/xmen2-pc`.

The original Motorola 68000 game logic was translated to static C/C++ and compiled natively for the N64's MIPS VR4300 CPU, completely eliminating interpreter and emulation overhead. Video and audio are handled directly on the Nintendo 64 hardware (RDP and RSP).

### Architecture and Features

- **Reference Variant (`heavy_60_opt`)**: Tuned for a rock-solid 60 FPS (60 Hz / 1-VBlank presentation pace on NTSC).
- **Native RDP Renderer**:
  - Unified priority plane extraction (`x2_plane_dual`).
  - Cached RDP command sequences for uniform scrolling planes (`X2_PLANE_CMD_CACHE`), minimizing CPU overhead.
  - Native RDP sprite packet rasterization with CI4 texture tiles and alpha masks.
  - Full hardware support for Genesis Shadow/Highlight color passes through multi-material RDP pipelines.
  - Triple buffering (`display_init` with 3 buffers at 16 bpp) decoupling simulation from frame presentation.
- **Native RSP Audio Engine**:
  - GEMS music sequencer rewritten natively in C++.
  - 8-voice PCM sample playback driven by the libdragon RSP audio mixer.
  - Pre-baked rate/pitch variants (`voices_v.pcm`), drastically minimizing cartridge PI DMA transfers.
  - Sliced audio mixing interleaved into the post-schedule display wait window, avoiding frame hitches.
- **Zero Telemetry**: Clean codebase free of diagnostic logging, counters, or runtime profiling probes, ensuring maximum execution performance.

---

### Current Status

- **Compatibility**: Verified on real Nintendo 64 hardware (EverDrive-64, SummerCart64) and cycle-accurate emulators (Ares v148+, simple64).
- **Frame Rate**: Smooth 60 FPS across complex stages and boss battles in the `heavy_60_opt` variant.
- **Visual Accuracy**: Authentic Planes A & B, sprite priorities, palette cycling, and Shadow/Highlight transparency effects.
- **Audio Fidelity**: Full musical score, sound effects, and digital speech samples rendered cleanly at 12 kHz / 16-bit stereo.

---

### Future Roadmap

1. **Translations**: Integrating custom text injection and language packs (e.g. Brazilian Portuguese).
2. **Save System**: Implementing persistent saving to cartridge battery/non-volatile memory (SRAM / EEPROM / FlashRAM) for stage progression.
3. **4-Player Multiplayer**: Verification and full enablement of 4-player cooperative gameplay routines (`game/runtime/players34.cpp`).

---

### Prerequisites & Build Environment

To build the ROM from source, ensure you have:

- **Operating System**:
  - Linux (Ubuntu 20.04/22.04/24.04 recommended) **OR**
  - Windows 10/11 using **WSL2** (Windows Subsystem for Linux - Ubuntu).
- **Host Tools**:
  - `build-essential` (`gcc`, `g++`, `make`)
  - `python3` (version 3.8+)
- **libdragon Toolchain & SDK**:
  - libdragon SDK installed (standard path `/opt/libdragon` or exported as `N64_INST`).
  - Cross-compiler utilities (`mips64-elf-gcc`, `mips64-elf-g++`, `mips64-elf-nm`, `mkdfs`, `n64tool`).
- **Original Game ROM**:
  - You must supply a clean, legitimate dump of the original Sega Genesis / Mega Drive cartridge.

---

### Input ROM Requirements

For legal reasons, this repository **DOES NOT contain any proprietary ROMs, artwork, or audio data**. You must supply your own ROM file dumped from your legally owned cartridge.

Place the ROM at:
`game/xmen2.bin`

#### ROM Specifications:
- **Game**: *X-Men 2: Clone Wars* (USA/Europe, Rev 00 / World)
- **Format**: Flat uncompressed binary (headerless, not SMD)
- **Exact Size**: 2,097,152 bytes (2 MiB)
- **Checksums**:
  - **SHA-1**: `61409e6cf6065ab67d8952b891d8edcf47777193`
  - **SHA-256**: `30bb09be17b15c9b86683967459fe3b9162f4d77dafab466b39c1b86254ee58f`
  - **MD5**: `b687660446f6e49dfd465cd0ef9f0bc3`

---

### Build Instructions

#### 1. Clone Repository

Clone recursively to fetch required submodules:

```bash
git clone --recursive https://github.com/Sunshinin11/X-men-2-clone-wars-64.git
cd X-men-2-clone-wars-64
```

If cloned non-recursively, run:
```bash
git submodule update --init --recursive
```

#### 2. Provide the ROM

Copy your verified Mega Drive ROM dump into `game/xmen2.bin`:
```bash
cp /path/to/your/rom.bin game/xmen2.bin
```

#### 3. Build

##### On Linux / WSL:
Run the build script:
```bash
chmod +x build_n64.sh
./build_n64.sh
```

##### On Windows (PowerShell):
If using Windows with WSL installed:
```powershell
powershell -ExecutionPolicy Bypass -File .\build_n64.ps1
```

#### 4. Build Output

Upon completion, the target ROM is generated in the project root:
- **`xmen2_heavy_60_opt.z64`**

Load it on Ares v148+, simple64, or copy it to your EverDrive-64 flashcart.

---

### Troubleshooting

- **Error `missing game ROM` or `short ROM`**:
  `game/xmen2.bin` is missing or not exactly 2,097,152 bytes. Verify your ROM dump against the expected hashes.
- **Error `command not found: mips64-elf-gcc`**:
  libdragon SDK is not in your environment. Install libdragon to `/opt/libdragon` or export `N64_INST=/path/to/libdragon`.
- **Line ending error (`\r: command not found`)**:
  If the repository was checked out with Windows CRLF line endings, run:
  ```bash
  dos2unix build_n64.sh
  ```
- **Error `submodule segagenesisrecomp not found`**:
  Run `git submodule update --init --recursive`.

---

### License & Attribution

- Port source code is distributed under the open-source licenses of `crabsmack/xmen2-pc` and `libdragon`.
- *X-Men 2: Clone Wars* is copyright and trademark of Sega Enterprises, Marvel Characters, Inc., and their respective owners.
- This project does not contain, distribute, or condone the unauthorized distribution of copyrighted material.
