# Luxor 3 Fix — Windows 10/11

**[Português](#português) · [English](#english)**

---

## Português

Correção não oficial que faz o **Luxor 3** (MumboJumbo, 2007 — versão Steam) abrir e rodar bem no Windows 10 e 11.

### O que corrige

| Problema | Causa | O que o fix faz |
|---|---|---|
| `ASSERTION FAILURE: TheVideoSystem.cpp(199) ... _numVidModes < MAX_VID_MODES` | Monitores atuais informam centenas de modos de vídeo (resolução × taxa de atualização); o jogo só tem espaço para poucos. | Mostra ao jogo uma lista curta: uma entrada de 32 bits por resolução. |
| `ASSERTION FAILURE: TheVideoSystem.cpp(319) ... DISP_CHANGE_SUCCESSFUL` / `0 hz (fullscreen)` | O jogo pede tela cheia com "0 Hz"; o Windows XP aceitava, o 10/11 recusa. | Pede 60 Hz (ou a taxa padrão, se 60 Hz não existir). |
| Engasgos / quedas de frames | O jogo controla o ritmo com `Sleep()`, que no Windows atual tem precisão de ~15,6 ms. | Ajusta o timer do sistema para 1 ms enquanto o jogo roda. |
| Centenas de FPS e imagem "rasgada" (tearing) | Os parâmetros de vídeo do jogo não ativam V-Sync no Windows atual. | Força V-Sync (tela cheia a 60 Hz; em janela, sincronizado com o monitor). |

### Instalação

1. Baixe o `Luxor3Fix-vX.Y.zip` em **Releases**.
2. Na Steam: botão direito em Luxor 3 → **Gerenciar** → **Ver arquivos locais**.
3. Copie o **`D3D8.dll`** do zip para essa pasta (ao lado do `Luxor3.exe`).
4. Se você ativou **modo de compatibilidade** no `Luxor3.exe`, desative (não é mais necessário).
5. Jogue. Tela cheia e janela funcionam (Options → Full Screen / Windowed).

**Desinstalar:** apague o `D3D8.dll` da pasta do jogo.

### Perguntas frequentes

- **Funciona com dgVoodoo2?** Sim, é opcional. Renomeie o `D3D8.dll` do dgVoodoo (pasta `MS\x86`) para `dgd3d8.dll` e coloque ao lado do jogo junto com o `dgVoodoo.conf`; o fix encaminha o Direct3D para ele.
- **Antivírus reclamou.** O fix é uma DLL sem assinatura digital que altera funções do Windows *apenas dentro do processo do jogo* — alguns antivírus estranham esse padrão. O código-fonte completo está em `src/`; você pode compilar a sua própria cópia (veja abaixo).
- **Verificar integridade dos arquivos na Steam remove o fix?** Não; a Steam não apaga arquivos extras. Mas se algo der errado, basta copiar o `D3D8.dll` de novo.
- **Reportar problema:** crie um arquivo vazio `luxorfix_test.txt` na pasta do jogo, abra o jogo (ele fecha sozinho após ~25 s) e envie o `luxorfix.log` gerado, junto com a mensagem de erro e o modelo do monitor/placa de vídeo. Apague o `luxorfix_test.txt` depois.

### Compilar

Requer Python 3 e o Zig (`python -m pip install ziglang`). Rode `build.bat`; o resultado sai em `dist\D3D8.dll`.

---

## English

Unofficial fix that makes **Luxor 3** (MumboJumbo, 2007 — Steam version) start and run smoothly on Windows 10 and 11.

### What it fixes

| Problem | Cause | Fix |
|---|---|---|
| `ASSERTION FAILURE: TheVideoSystem.cpp(199) ... _numVidModes < MAX_VID_MODES` | Modern monitors report hundreds of display modes (resolution × refresh rate); the game has room for only a few. | Reports a short list to the game: one 32-bit entry per resolution. |
| `ASSERTION FAILURE: TheVideoSystem.cpp(319) ... DISP_CHANGE_SUCCESSFUL` / `0 hz (fullscreen)` | The game requests fullscreen at "0 Hz"; Windows XP accepted it, 10/11 reject it. | Requests 60 Hz (or the default rate if 60 Hz is unavailable). |
| Stutter / frame drops | The game paces frames with `Sleep()`, which has ~15.6 ms granularity on modern Windows. | Raises the system timer resolution to 1 ms while the game runs. |
| Hundreds of fps and tearing | The game's Direct3D settings don't enable vsync on modern Windows. | Forces vsync (60 Hz fullscreen; synced to the monitor in windowed mode). |

### Install

1. Download `Luxor3Fix-vX.Y.zip` from **Releases**.
2. In Steam: right-click Luxor 3 → **Manage** → **Browse local files**.
3. Copy **`D3D8.dll`** from the zip into that folder (next to `Luxor3.exe`).
4. If you enabled a **compatibility mode** on `Luxor3.exe`, turn it off (no longer needed).
5. Play. Fullscreen and windowed both work (Options → Full Screen / Windowed).

**Uninstall:** delete `D3D8.dll` from the game folder.

### FAQ

- **dgVoodoo2?** Optional and supported: rename dgVoodoo's `MS\x86\D3D8.dll` to `dgd3d8.dll`, place it next to the game with `dgVoodoo.conf`, and the fix forwards Direct3D to it.
- **Antivirus warning?** The fix is an unsigned DLL that patches Windows functions *inside the game process only*, a pattern some antivirus heuristics dislike. Full source is in `src/`; you can build it yourself.
- **Reporting a problem:** create an empty `luxorfix_test.txt` in the game folder, start the game (it closes itself after ~25 s), and send the resulting `luxorfix.log` with the error message and your monitor/GPU model. Delete `luxorfix_test.txt` afterwards.

### Build

Requires Python 3 and Zig (`python -m pip install ziglang`). Run `build.bat`; output goes to `dist\D3D8.dll`.

---

Not affiliated with MumboJumbo or Valve. Luxor is a trademark of its respective owners. No game files are modified or distributed.
