<div align="center">

# X-Men 2: Clone Wars — Nintendo 64

**A native Nintendo 64 port of the Sega Genesis / Mega Drive classic.**  
**Um port nativo de X-Men 2: Clone Wars para Nintendo 64.**

![Nintendo 64](https://img.shields.io/badge/platform-Nintendo%2064-20252b?style=flat-square)
![libdragon](https://img.shields.io/badge/SDK-libdragon-0d9488?style=flat-square)
![Status](https://img.shields.io/badge/status-work%20in%20progress-f59e0b?style=flat-square)
![ROM](https://img.shields.io/badge/game%20ROM-not%20included-52525b?style=flat-square)

**[Português](#pt-br) · [English](#en) · [Contributing](CONTRIBUTING.md) · [Legal notices](NOTICE.md)**

</div>

---

| Siberia / Sibéria | Character select / Seleção |
| :---: | :---: |
| ![Siberia](docs/media/siberia.png) | ![Character select](docs/media/select.png) |
| Savage Land / Terra Selvagem | Magneto |
| ![Savage Land](docs/media/savage-land.png) | ![Magneto](docs/media/magneto.png) |

<sub>Images already in this repository are illustrative; they are not hardware-verification evidence. / Imagens ilustrativas existentes no repositório; não comprovam execução no console real.</sub>

<a id="pt-br"></a>
## 🇧🇷 Português (Brasil)

### O projeto

Este projeto leva **X-Men 2: Clone Wars** (Mega Drive / Genesis) ao **Nintendo 64**, sem depender da emulação completa do console original.

A lógica do jogo foi traduzida de Motorola 68000 para C/C++ estático e compilada para a CPU **MIPS VR4300** do N64. A plataforma usa **libdragon**, com renderização de sprites e planos pelo **RDP** e reprodução de áudio pelo **RSP**. O trabalho parte da engenharia reversa de [crabsmack/xmen2-pc](https://github.com/crabsmack/xmen2-pc).

O objetivo é preservar o jogo e abrir espaço para a comunidade estudar, corrigir e aperfeiçoar o port.

### Estado atual

| Recurso | Situação |
| --- | --- |
| Execução nativa no N64 | Implementada; ROM gerada por compilação cruzada MIPS |
| Renderização | RDP nativo, planos com prioridades, sprites CI4 e efeitos de sombra/destaque |
| Áudio | Sequenciador GEMS nativo, amostras PCM e mixer RSP de 8 vozes |
| Desempenho | **59,41 FPS de média** em uma sessão de 6.600 quadros no Ares, não 60 FPS absolutamente travados |
| N64 real | Compatibilidade pretendida; a medição acima é de emulador e precisa de validação adicional no hardware |
| Cooperativo de 4 jogadores | Código experimental presente em `game/runtime/players34.cpp`; testes completos pendentes |
| Save | **Não implementado**; o jogo original não possui salvamento |
| Tradução PT-BR | **Planejada**, não incluída nesta versão |

**Sobre os números:** a medição é de uma sessão da variante `heavy_60_opt`, no Ares com paraLLEl-RDP, em 5 de novembro de 2026. O ritmo usa **1 VBlank por quadro**; foram observadas janelas próximas de 59,82 FPS e uma janela mínima de aproximadamente 55,65 FPS. Esses números não garantem a mesma taxa em todas as fases nem no N64 real.

### Arquitetura

- **CPU:** código de jogo recompilado estaticamente para MIPS VR4300, com rotinas de ponte adaptadas ao N64.
- **Vídeo:** RDP da libdragon, cache de comandos dos planos de rolagem uniforme, prioridades de sprites e materiais de sombra/destaque.
- **Áudio:** sequenciador GEMS em C++, oito vozes PCM no RSP e processamento fatiado durante a espera pela apresentação.
- **Memória:** requer **Expansion Pak (8 MiB de RDRAM)**.
- **Variante de referência:** `xmen2_heavy_60_opt.z64`.

A compilação verifica símbolos proibidos de subsistemas de emulação e valida os índices das amostras de áudio. Confira `tools/n64_native_audit.py` e a documentação técnica em [platform/n64](platform/n64/).

### Compilar a ROM

> **O jogo original não acompanha este repositório.** É obrigatório fornecer um dump próprio do cartucho de Mega Drive/Genesis, compatível com o hash esperado.

**Requisitos:** Linux ou Windows com WSL2, Git, Python 3, `make`, compilador C/C++ do host, SDK/toolchain MIPS da **libdragon** (por padrão em `/opt/libdragon`) e os submódulos Git.

**1. Clonar com os submódulos:**

~~~bash
git clone --recursive https://github.com/Sunshinin11/X-men-2-clone-wars-64.git
cd X-men-2-clone-wars-64
~~~

**2. Fornecer o dump original** em `game/xmen2.bin`:

| Propriedade | Valor esperado |
| --- | --- |
| Jogo | X-Men 2: Clone Wars (USA/Europe, Rev 00 / World) |
| Formato | BIN puro, sem cabeçalho SMD |
| Tamanho | **2.097.152 bytes (2 MiB)** |
| SHA-1 | `61409e6cf6065ab67d8952b891d8edcf47777193` |
| SHA-256 | `30bb09be17b15c9b86683967459fe3b9162f4d77dafab466b39c1b86254ee58f` |

O script confere o SHA-1 antes da compilação. Para usar outro caminho, defina `XMEN2_ROM` apontando para seu dump.

**3. Compilar:**

Linux / WSL:

~~~bash
./build_n64.sh
~~~

Windows / PowerShell, com WSL2 configurado:

~~~powershell
.\build_n64.ps1
~~~

**4. Resultado:** `xmen2_heavy_60_opt.z64`, na raiz do repositório. Use um emulador compatível, como **Ares v148+**, ou um flashcart compatível no N64 com Expansion Pak.

Se algum submódulo estiver ausente, execute `git submodule update --init --recursive`. Se a compilação reclamar de `mips64-elf-g++`, confira a instalação da libdragon e a variável `N64_INST`.

### Próximos passos

- Validar **quatro controles simultâneos**, seleção, continues, mortes e transições de fase no N64.
- Acrescentar **save persistente de progresso entre fases**, sem modificar desnecessariamente o funcionamento original.
- Implementar **tradução PT-BR** e revisar fontes/caracteres.
- Testar mais fases e chefes, sincronização, música e efeitos no **N64 real**.
- Receber correções, documentação e otimizações reproduzíveis da comunidade.

Quer contribuir? Leia [CONTRIBUTING.md](CONTRIBUTING.md) e abra uma issue ou pull request. **Não envie ROMs comerciais nem recursos extraídos do jogo.**

### Créditos e licenças

Port para N64: [Sunshinin11](https://github.com/Sunshinin11) e colaboradores. Base de engenharia reversa: [crabsmack/xmen2-pc](https://github.com/crabsmack/xmen2-pc). SDK: [libdragon](https://github.com/DragonMinded/libdragon).

O código original deste repositório tem licença MIT, mas **componentes de terceiros possuem licenças próprias**, incluindo uma dependência **PolyForm Noncommercial**. Consulte [LICENSE](LICENSE) e [NOTICE.md](NOTICE.md) antes de redistribuir ou reutilizar. Nenhum direito sobre o jogo original é concedido; X-Men e seus personagens pertencem aos respectivos titulares.

---

<a id="en"></a>
## 🇬🇧 English

### About

**X-Men 2: Clone Wars** for the Sega Genesis / Mega Drive, ported to the **Nintendo 64** without running a complete Genesis emulator.

The original Motorola 68000 gameplay logic has been statically translated to C/C++ and cross-compiled for the N64's **MIPS VR4300**. The platform uses **libdragon**, the **RDP** for native 2D rendering, and the **RSP** for sampled audio. It builds on the reverse-engineering work of [crabsmack/xmen2-pc](https://github.com/crabsmack/xmen2-pc).

The goal is to keep the project open for others to learn from, improve and extend.

### Project status

| Area | Status |
| --- | --- |
| Native N64 execution | Implemented; MIPS cross-compiled ROM |
| Graphics | Native RDP renderer; priority planes, CI4 sprites, shadow/highlight effects |
| Audio | Native GEMS sequencer, PCM sample assets, eight-voice RSP mixer |
| Performance | **59.41 FPS average** over a 6,600-frame Ares session; not a perfect locked 60 |
| Real N64 | Intended target; the performance figures above are from emulation and require additional console validation |
| Four-player co-op | Experimental support in `game/runtime/players34.cpp`; complete playtesting pending |
| Save system | **Not implemented**; the original game has no save feature |
| Brazilian Portuguese | **Planned**, not shipped in this build |

**Performance note:** these figures come from a `heavy_60_opt` run using Ares with paraLLEl-RDP on November 5, 2026. The game targets **one VBlank per presented frame**. Many sampled windows were around 59.82 FPS; the slowest window was approximately 55.65 FPS. Results may vary by scene and are not a real-hardware guarantee.

### How it works

- **CPU:** statically compiled MIPS VR4300 gameplay logic and N64-specific bridge code.
- **Video:** libdragon RDP, cached commands for scrolling planes, sprite priorities and shadow/highlight materials.
- **Audio:** native C++ GEMS sequencer, eight-voice PCM mixing on the RSP and sliced audio work during presentation pacing.
- **Memory:** **Expansion Pak required (8 MiB RDRAM)**.
- **Reference output:** `xmen2_heavy_60_opt.z64`.

The build includes native-link checks for prohibited emulation symbols and validates the packed audio variant index. See `tools/n64_native_audit.py` and [platform/n64](platform/n64/) for technical details.

### Build your own ROM

> **No commercial game ROM or original game assets are distributed here.** You must supply your own compatible Genesis/Mega Drive cartridge dump.

**Prerequisites:** Linux or Windows with WSL2, Git, Python 3, `make`, a host C/C++ compiler, the **libdragon** N64 SDK/MIPS toolchain (default: `/opt/libdragon`), and the Git submodules.

**1. Clone recursively:**

~~~bash
git clone --recursive https://github.com/Sunshinin11/X-men-2-clone-wars-64.git
cd X-men-2-clone-wars-64
~~~

**2. Supply the original game dump** at `game/xmen2.bin`:

| Property | Required value |
| --- | --- |
| Game | X-Men 2: Clone Wars (USA/Europe, Rev 00 / World) |
| Format | Headerless, flat BIN (not SMD) |
| Size | **2,097,152 bytes (2 MiB)** |
| SHA-1 | `61409e6cf6065ab67d8952b891d8edcf47777193` |
| SHA-256 | `30bb09be17b15c9b86683967459fe3b9162f4d77dafab466b39c1b86254ee58f` |

The build verifies the SHA-1. To use a different local path, set the `XMEN2_ROM` environment variable.

**3. Build:**

Linux / WSL:

~~~bash
./build_n64.sh
~~~

Windows / PowerShell (WSL2 required):

~~~powershell
.\build_n64.ps1
~~~

**4. Output:** `xmen2_heavy_60_opt.z64` in the repository root. Run it in a compatible emulator such as **Ares v148+**, or on an Expansion Pak-equipped N64 with a suitable flashcart.

If Git submodules are missing, run `git submodule update --init --recursive`. If the compiler is missing, check your libdragon installation and `N64_INST`.

### Roadmap

- Verify **four-controller cooperative play**, character selection, continues, deaths and stage transitions on N64.
- Add **persistent between-stage progress saving** without changing the original gameplay unnecessarily.
- Implement **Brazilian Portuguese localization**, including font coverage.
- Validate more stages, bosses, synchronization and audio on **real N64 hardware**.
- Welcome documented community optimizations and bug fixes.

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. **Never attach commercial ROMs or extracted game assets.**

### Credits, licensing and legal notice

N64 port: [Sunshinin11](https://github.com/Sunshinin11) and contributors. Reverse-engineering foundation: [crabsmack/xmen2-pc](https://github.com/crabsmack/xmen2-pc). SDK: [libdragon](https://github.com/DragonMinded/libdragon).

Original repository code is MIT-licensed, but **third-party components retain their respective licenses**, including a **PolyForm Noncommercial** dependency. Read [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md) before redistribution or reuse. No rights to the original game are granted; X-Men and its characters belong to their respective owners.
