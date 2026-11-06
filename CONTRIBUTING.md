# Contributing / Como contribuir

Thank you for helping improve the X-Men 2: Clone Wars Nintendo 64 port. This is a community-oriented, source-only project.

Obrigado por ajudar a melhorar o port de X-Men 2: Clone Wars para Nintendo 64. Este é um projeto comunitário que distribui apenas código-fonte.

## Before opening a pull request / Antes de abrir um PR

- **Keep game behavior intact / Preserve o funcionamento original.** Avoid changing movement, timing, enemy behavior or audio unless the change is intentional and explained.
- **Measure performance / Meça o desempenho.** For optimization proposals, provide the baseline and updated build, scene or stage, number of frames, emulator/hardware, FPS and frame-time samples. Do not infer a hardware speedup from host-only timings.
- **Report real hardware separately / Separe hardware real de emulador.** Include console model, Expansion Pak, flashcart, and emulator version when applicable.
- **Test multiplayer / Teste o multiplayer.** If touching input, sprites, stage transitions, or gameplay logic, check one-, two-, three- and four-player paths where possible.
- **No proprietary data / Sem dados proprietários.** Never commit or attach `game/xmen2.bin`, `*.z64`, `*.dfs`, `*.pcm`, `*.idx`, copyrighted game audio/images, or packaged ROM files.
- **Keep builds reproducible / Mantenha o build reproduzível.** Follow the README, use the pinned submodules, and explain any new host dependencies.
- **Respect upstream licenses / Respeite as licenças.** The original project code is MIT-licensed, while dependencies include noncommercial and other licenses; see [NOTICE.md](NOTICE.md).

## Bug reports / Relatórios de bugs

Please include / Inclua:

1. Affected commit and ROM build name / Commit e nome do build afetados.
2. Ares or real N64, hardware/flashcart and Expansion Pak / Emulador ou hardware real e acessórios.
3. Stage, character and number of players / Fase, personagem e número de jogadores.
4. Steps to reproduce and expected vs. observed result / Passos para reproduzir e resultado esperado vs. observado.
5. Log excerpts or screenshots **without** proprietary game files / Trechos de log ou capturas sem distribuir dados do jogo.

## Suggested contribution areas / Áreas para colaborar

- Four-player mode validation / Validação do modo para quatro jogadores.
- Save system / Sistema de salvamento.
- Brazilian Portuguese localization / Tradução PT-BR.
- Gameplay, audio and graphics compatibility / Compatibilidade de gameplay, áudio e gráficos.
- Performance measurements on actual Nintendo 64 hardware / Medições de desempenho no N64 real.
- Build guides and documentation / Guias de compilação e documentação.

For larger changes, open an issue first to align on the approach. / Para mudanças maiores, abra uma issue antes de começar, para alinhar a abordagem.
