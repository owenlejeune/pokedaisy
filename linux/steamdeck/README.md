# PokeDaisy on Steam Deck (RetroDECK, Tender & SteamOS)

Guide for running PokeDaisy natively on Steam Deck (`1280x800` borderless side-by-side companion + touch + back paddles) as a standalone AppImage, inside RetroDECK, with Tender save sync, or via the libretro core.

## 1. Install the Standalone AppImage

Download `pokedaisy-linux-x86_64.AppImage` and place it at `/home/deck/Applications/pokedaisy.AppImage`:

```bash
mkdir -p ~/Applications
mv ~/Downloads/pokedaisy-linux-x86_64.AppImage ~/Applications/pokedaisy.AppImage
chmod +x ~/Applications/pokedaisy.AppImage
```

By default, standalone mode uses XDG paths (`~/.local/share/pokedaisy/`, `~/.config/pokedaisy/settings.ini`) and stem-based save filenames (`<rom-stem>.sav`).

## 2. Steam Gaming Mode (Non-Steam Shortcut)

- Add `/home/deck/Applications/pokedaisy.AppImage` as a Non-Steam Game.
- Recommended Launch Options: `--fullscreen` (add `"<rom-path>"` to boot directly into a game, or omit to open the built-in ROM picker).
- Optional Steam Grid artwork (`grid-vertical.png`, `grid-horizontal.png`, `hero.png`, `logo.png`) is in [`artwork/`](artwork/) (and in `steam-grid/` inside `PokeDaisy.AppDir` / `--appimage-extract`).
- **Display:** On `1280x800` (`SETTINGS > LAYOUT: AUTO`), the game renders at 4x on the left (`960x800` pane) and the live companion sidebar stays visible on the right (`320x800`).
- **Touch & Trackpad:** Tap or drag directly on the sidebar tabs/buttons.
- **Deck Controls (`Gamepad` template):**
  - `L4` (`PADDLE2`): Toggle controller focus between game and companion sidebar (`X` / `Tab`)
  - `R4` (`PADDLE1`): Toggle fast-forward
  - `R5` (`PADDLE3`): Open Quick Menu (`Y` / `F`)
  - `L5` (`PADDLE4`): Save state (`Select + L5` loads state)
  - `Select + Start`: Pause menu
  - `SETTINGS > FACE BUTTONS`: Switch between `NINTENDO` (`East=A`, default) and `XBOX` (`South=A`).

## 3. RetroDECK (ES-DE Custom System)

RetroDECK runs inside a Flatpak sandbox, so ES-DE uses `flatpak-spawn --host` to launch the host AppImage.

1. Copy [`es_systems.xml`](es_systems.xml) into your RetroDECK ES-DE custom systems directory:
   - `~/retrodeck/ES-DE/custom_systems/es_systems.xml` (or `~/.var/app/net.retrodeck.retrodeck/config/ES-DE/custom_systems/es_systems.xml`)
   - Command used:
     ```text
     flatpak-spawn --host /home/deck/Applications/pokedaisy.AppImage --retrodeck --fullscreen "%ROM%"
     ```
2. Select **PokeDaisy (Standalone)** as the alternate emulator for GBA in ES-DE.
3. `--retrodeck` automatically points saves to `~/retrodeck/saves/gba/` (or `~/.var/app/net.retrodeck.retrodeck/data/saves/gba/`), save states to `~/retrodeck/states/gba/`, uses stem naming (`Emerald.sav`), isolates PokeDaisy startup backups to `~/.local/share/pokedaisy/backups/`, and changes the pause menu quit entry to **Quit game**.
4. **Uncompressed vs `.zip`/`.7z` ROMs:** Because PokeDaisy's pinned mGBA build sets `USE_LIBZIP=OFF`, `es_systems.xml` restricts `<extension>` to `.gba .GBA`. If your library uses `.zip` or `.7z` archives, copy [`pokedaisy-esde-wrapper.sh`](pokedaisy-esde-wrapper.sh) to `~/Applications/`, `chmod +x` it, and invoke it in `<command>` instead.
5. For `gb`/`gbc` custom systems under RetroDECK, pass `--save-dir /home/deck/retrodeck/saves/gb` (or `gbc`) and `--state-dir ...` so saves do not go into `saves/gba/`.

## 4. Tender Bidirectional Save Sync

Configure Tender's custom GBA runner to invoke PokeDaisy with Tender's managed save directory:

```text
~/Applications/pokedaisy.AppImage --fullscreen --save-dir <tender_save_dir> "%ROM%"
```

Notes for Tender compatibility:
- **Flush-on-write:** PokeDaisy calls `pocket_save_flush()` (`msync(MS_SYNC)` + `fsync()`) as soon as an in-game save completes (`dirty` `1 -> 0`), as well as on pause, state save, and exit, so `inotify` sees updated `.sav` bytes without waiting for ROM unload.
- **`.srm` saves:** Defaults to `.sav`. Pass `--save-ext srm` to always use `.srm`; if omitted and `<stem>.sav` is missing while `<stem>.srm` already exists in `--save-dir`, PokeDaisy opens the existing `.srm` automatically.
- **Persistent config / env:** Instead of passing `--save-dir` each time, you can set `POKEDAISY_SAVE_DIR=<tender_save_dir>` or `save_dir=<tender_save_dir>` in `~/.config/pokedaisy/settings.ini`.

## 5. Libretro Core Option (`pokedaisy_libretro.so`)

If running `pokedaisy_libretro.so` directly inside RetroDECK/RetroArch instead of the standalone AppImage, open **Core Options** and set **Panel size (`pokedaisy_panel`)** to `1280x800` for native Steam Deck integer scaling.

