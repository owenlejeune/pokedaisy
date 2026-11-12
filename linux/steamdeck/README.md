# PokeDaisy on Steam Deck (RetroDECK, Tender & SteamOS)

Guide for running PokeDaisy natively on Steam Deck (`1280x800` borderless side-by-side companion + touch + back paddles) as a standalone AppImage, inside RetroDECK, with Tender save sync, or via the libretro core.

## 1. Install the Standalone AppImage

Download `pokedaisy-linux-x86_64.AppImage` and place it at `/home/deck/Applications/pokedaisy.AppImage`:

```bash
mkdir -p ~/Applications
mv ~/Downloads/pokedaisy-linux-x86_64.AppImage ~/Applications/pokedaisy.AppImage
chmod +x ~/Applications/pokedaisy.AppImage
```

> [!TIP]
> If SteamOS says `fusermount3: too many fuse filesystems mounted`:
> - Run with `APPIMAGE_EXTRACT_AND_RUN=1 ~/Applications/pokedaisy.AppImage` (bypasses FUSE).
> - Or extract once for zero FUSE overhead:
>   ```bash
>   cd ~/Applications && ./pokedaisy.AppImage --appimage-extract && mv squashfs-root pokedaisy
>   ```
>   Launch target: `/home/deck/Applications/pokedaisy/AppRun`.

By default, standalone mode uses XDG paths (`~/.local/share/pokedaisy/`, `~/.config/pokedaisy/settings.ini`) and stem-based save filenames (`<rom-stem>.sav`).

## 2. Steam Gaming Mode (Non-Steam Shortcut)

- Add `/home/deck/Applications/pokedaisy.AppImage` as a Non-Steam Game.
- Recommended Launch Options: `--fullscreen` (add `"<rom-path>"` to boot directly into a game, or omit to open the built-in ROM picker).
- Optional Steam Grid artwork (`grid-vertical.png`, `grid-horizontal.png`, `hero.png`, `logo.png`) is in [`artwork/`](artwork/) (and in `steam-grid/` inside `PokeDaisy.AppDir` / `--appimage-extract`).
- **Display:** On `1280x800` (`LAYOUT: AUTO`), the game renders on the left (`960x800` pane, 4x GBA `960x640` ORIGINAL or `960x800` EXPAND) and the live companion sidebar stays visible on the right (`320x800`). Toggle anytime between `AUTO`, `SIDE BY SIDE`, and `OVERLAY` in Companion (`X`) -> **SETTINGS > LAYOUT**, in the host menu (`Select + Start`) -> **LAYOUT**, or via `--layout side-by-side|overlay|auto`.
- **Touch & Trackpad:** Tap or drag directly on the live sidebar (`x >= 960` on `1280x800`) even while the game keeps running.
- **Deck Controls (`Gamepad` template):**
  - `Select + Start`: Host pause menu (**LAYOUT**, **A/B BUTTONS**, **HOTKEYS**, **REWIND**, **FF SPEED**, **LCD FILTER**, **SAVE/LOAD STATE**)
  - `X` (`Tab`): Toggle controller focus between game and companion sidebar (or open/close companion in `OVERLAY` mode)
  - `Y` (`F`): Open Quick Menu
  - `A/B BUTTONS`: Cycle between `NORMAL` (`Nintendo` layout) and `SWAPPED` (`Xbox` layout) in the `Select + Start` menu.
  - Back paddles (when not rebound in `Select + Start` -> `HOTKEYS`): `PADDLE 1` fast-forward (hold), `PADDLE 2` companion/sidebar toggle (`X`), `PADDLE 3` Quick Menu (`Y`), `PADDLE 4` save state (`Select + PADDLE 4` loads state).

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
- **Flush-on-write:** PokeDaisy checks SRAM every ~0.5s (30 frames) and writes atomically with `fsync()` via `sram_write()`, as well as immediately on pause (`Select + Start`), state save, and exit, so `inotify` sees updated `.sav` bytes without waiting for ROM unload.
- **`.srm` saves:** Defaults to `.sav`. Pass `--save-ext srm` to always use `.srm`; if omitted and `<stem>.sav` is missing while `<stem>.srm` already exists in `--save-dir`, PokeDaisy opens the existing `.srm` automatically.
- **Persistent config / env:** Instead of passing `--save-dir` each time, you can set `POKEDAISY_SAVE_DIR=<tender_save_dir>` or `save_dir=<tender_save_dir>` in `~/.config/pokedaisy/settings.ini`.

## 5. Libretro Core Options (`pokedaisy_gba_libretro.so` / `pokedaisy_gbc_libretro.so`)

If running `pokedaisy_gba_libretro.so` or `pokedaisy_gbc_libretro.so` directly inside RetroDECK/RetroArch instead of the standalone AppImage, open **Core Options** and set:
- **Panel size (`pokedaisy_panel`)**: `1280x800` for native Steam Deck scaling and automatic 2-pane side-by-side mode.
- **Companion layout (`pokedaisy_layout`)**: `auto`, `side-by-side`, or `overlay`.

