# Steam Deck & Linux Port Implementation Plan (RetroDECK / Tender)

## 1. Codebase Baseline vs. Porting Guidelines

An audit of the repository shows that the core Linux and Libretro foundations already exist, so the Steam Deck port builds on existing targets rather than starting from scratch:

| Guideline Assumption | Actual Repository State | Plan Adjustment |
| :--- | :--- | :--- |
| Needs a new CMake + SDL2/ImGui/Skia host (`pokedaisy-host`) | [`linux/CMakeLists.txt`](../../linux/CMakeLists.txt) already builds `pokedaisy-linux` (C11, SDL2, `USE_PTHREADS`, static `third_party/mgba`, software pixel-art companion from `3ds/core/`, `pk_wide` EXPAND, `pk_rewind`). | Extend `pokedaisy-linux` directly; no ImGui, Skia, or Android JNI replacement is needed. |
| Cannot be a Libretro core (`.so`) | [`libretro/CMakeLists.txt`](../../libretro/CMakeLists.txt) and [`libretro/pd_libretro.c`](../../libretro/pd_libretro.c) already build `pokedaisy_libretro.so` (plus [`linux/libretro-ds/`](../../linux/libretro-ds) for DS). | Use **Standalone AppImage (`pokedaisy-linux`) as Track A** (full control over 16:10 side-by-side layout, CLI flags, Gamescope, and Tender) and optionally add a `1280x800` panel preset to `pd_libretro.c` as Track B. |
| Config uses `~/.config/pokedaisy/config.toml` | [`3ds/core/pd_settings.c`](../../3ds/core/pd_settings.c) already parses/saves `key=value` `settings.ini`. | Use XDG `~/.config/pokedaisy/settings.ini` to match existing settings persistence. |
| `.sav` matches ROM stem | [`linux/main.c`](../../linux/main.c) currently writes `DATA/saves/<rom_filename>.gba.sav` (keeps extension). | **Critical fix:** strip ROM extension (`<rom_stem>.sav` / `.srm`) for Tender and RetroDECK compatibility. |

---

## 2. Target Tri-Mode Architecture

```
                                      +---------------------------------------------+
                                      |                 RomM Server                 |
                                      +----------------------+----------------------+
                                                             | (API sync)
                                                             v
+-------------------------------+                  +-------------------+
|       Steam Game Mode         | <--------------> |   Tender Plugin   |
|   (Direct Non-Steam Shortcut) |                  |   (Decky Loader)  |
+---------------+---------------+                  +---------+---------+
                |                                            |
                | (Direct CLI args)                          | (Save symlink / paths)
                v                                            v
+-------------------------------+                  +-------------------+
|      PokeDaisy Standalone     | <--------------> |  Shared Save Pool |
|   (pokedaisy-linux AppImage)  |                  | (~/.../saves/gba) |
+---------------+---------------+                  +---------+---------+
                ^                                            ^
                | (flatpak-spawn --host or custom runner)    |
+---------------+---------------+                            |
|       RetroDECK (ES-DE)       | ---------------------------+
|      (Optional Frontend)      |
+-------------------------------+
```

---

## 3. Phased Implementation Plan

### Phase 1 — CLI, XDG Paths & Save Naming
**Files:** [`linux/main.c`](../../linux/main.c), [`linux/host_io.c`](../../linux/host_io.c), [`linux/host_io.h`](../../linux/host_io.h)

1. **Expand CLI flags in `main()`:**
   - Add `--fullscreen` / `-f`, `--save-dir <PATH>`, `--state-dir <PATH>`, `--layout <side-by-side|overlay|auto>`, `--retrodeck`, `--help`, and `--version`, while preserving `--data`, `--save`, `--frames`, and `--shot`.
   - When `--data` is omitted, default to XDG Base Directory paths instead of `"."`:
     - Config: `$XDG_CONFIG_HOME/pokedaisy/settings.ini` (`~/.config/pokedaisy/settings.ini`)
     - Saves/States fallback: `$XDG_DATA_HOME/pokedaisy/{saves,states}` (`~/.local/share/pokedaisy/{saves,states}`)
2. **Implement cascading save/state resolution:**
   - **Save directory priority:** `--save-dir` > `$POKEDAISY_SAVE_DIR` > `--retrodeck` (`~/retrodeck/saves/gba` or `~/.var/app/net.retrodeck.retrodeck/data/saves/gba` if present) > `save_dir=` in `settings.ini` > `--data`/XDG fallback.
   - **State directory priority:** `--state-dir` > `$POKEDAISY_STATE_DIR` > `--retrodeck` (`~/retrodeck/states/gba`) > `state_dir=` in `settings.ini` > `--data`/XDG fallback.
3. **Fix battery save filename convention:**
   - Strip `.gba`/`.gbc`/`.gb`/`.nds` when resolving `sav` in non-`--data` modes (or add `--save-naming stem|full`, defaulting to `stem` when `--save-dir`, `--retrodeck`, or XDG mode is active so Tender's `<ROM_STEM>.sav` matches out of the box).
   - Keep `pocket_save_backup()` writing rolling backups to a separate directory (`~/.local/share/pokedaisy/backups/` or `DATA/pokedaisy-backups/`) so backup files never pollute Tender's watched save directory.

---

### Phase 2 — 16:10 Side-by-Side Layout & Live Sidebar
**Files:** [`linux/main.c`](../../linux/main.c), [`3ds/core/pd_ui.c`](../../3ds/core/pd_ui.c), [`3ds/core/pd_ui_party.c`](../../3ds/core/pd_ui_party.c), [`3ds/core/pd_ui_battle_controls.c`](../../3ds/core/pd_ui_battle_controls.c), [`3ds/core/pd_settings.h`](../../3ds/core/pd_settings.h), [`3ds/core/pd_settings.c`](../../3ds/core/pd_settings.c)

1. **Remove `320×240` / `768px` height constraints in `pd_ui`:**
   - Raise `CANVAS_MAX_W` / `CANVAS_MAX_H` in `linux/main.c` (currently `1024×768`, which cannot hold an `800px` tall sidebar).
   - Replace or guard the fixed `PD_UI_WIDTH * PD_UI_HEIGHT` (`320×240`) static scratch buffers in `draw_cards()` (`3ds/core/pd_ui_party.c`) and `saved[]` (`3ds/core/pd_ui_battle_controls.c`) so tall canvases (`320×800`) do not overflow.
2. **Add layout mode to `pd_settings`:**
   - Add `layout` (`auto`, `side-by-side`, `overlay`) to `struct pd_settings` and `pd_settings_load()`/`pd_settings_save()`.
   - In `auto` mode, select `side-by-side` when viewport width >= `1280` and aspect >= 16:10 (e.g. `1280×800` Steam Deck), falling back to `overlay` on smaller/4:3 handheld panels.
3. **Render dual-pane viewport (`1280×800`):**
   - **Left pane (`960×800`):**
     - `ORIGINAL` aspect: `960×640` (4× GBA integer scale) centered vertically (`y = 80`).
     - `EXPAND` aspect: invoke `wide_window(960, 800, ...)` -> renders `240×200` expanded overworld at 4× (`960×800` full left pane, within `PK_WIDE_MAX_VIEW_H = 800`).
   - **Right pane (`320×800` at `x = 960`):**
     - Render `pd_ui_draw()` into the right-side canvas and composite onto `renderer` alongside `render_game()` **without pausing `c->runFrame(c)`**.
     - Keep `menuOpen` (Game Menu / confirm dialogs) and `quickOpen` (Quick Menu) overlaid cleanly over the game pane or centered.
4. **Input focus routing (`focusSidebar`):**
   - Keep emulation running continuously; route D-pad/A/B to mGBA during normal play, or to the sidebar when `focusSidebar` is toggled (`X`/Tab, `L3`, or Deck back grip) or when touched/clicked.

---

### Phase 3 — Steam Deck Display, Touch & Controller Input
**Files:** [`linux/main.c`](../../linux/main.c)

1. **Gamescope-friendly fullscreen:**
   - Use `SDL_WINDOW_FULLSCREEN_DESKTOP` instead of `SDL_WINDOW_FULLSCREEN` when `--fullscreen` or `POCKET_FULLSCREEN=1` is set to avoid Wayland/Gamescope mode-setting glitches.
2. **Touchscreen & trackpad support:**
   - Wire `SDL_MOUSEBUTTONDOWN` / `SDL_MOUSEBUTTONUP` / `SDL_MOUSEMOTION` and `SDL_FINGERDOWN` / `SDL_FINGERUP` / `SDL_FINGERMOTION` into `pd_ui_touch()`, mapping window coordinates into the right-pane sidebar (`(x - 960) / scale`).
3. **Steam Deck gamepad & hotkeys:**
   - Map `SDL_CONTROLLER_BUTTON_PADDLE1..4` (L4/R4/L5/R5) for Steam Input / standalone desktop mode (sidebar focus toggle, fast-forward, quick menu, state save/load).
   - Standardize `--retrodeck` quit/menu hotkeys (`Select + Start`) and make the Nintendo vs. Xbox AB/XY face-button swap configurable in `pd_settings` (currently hardcoded Nintendo swap in `controller_button()`).

---

### Phase 4 — Tender Bidirectional Save Sync Hardening
**Files:** [`linux/main.c`](../../linux/main.c), [`linux/host_io.c`](../../linux/host_io.c)

1. **Flush-on-Write for `inotify` detection:**
   - Monitor `pk_save_dirty(c)` transitions in the main frame loop; when an in-game save finishes (`dirty` `1 -> 0`) or on pause, state save, or exit, call `msync(..., MS_SYNC)` / `fsync()` via a `pocket_save_flush()` helper so Tender detects updated `.sav` bytes immediately without waiting for `c->unloadROM(c)`.
2. **Optional `.srm` compatibility:**
   - Add `--save-ext sav|srm` (default `.sav`), with optional fallback detection if a matching `.srm` already exists in the target save directory.

---

### Phase 5 — RetroDECK & Tender Integration Assets
**Files:** new `linux/steamdeck/` templates + optional [`libretro/pd_libretro.c`](../../libretro/pd_libretro.c)

1. **RetroDECK ES-DE custom system snippet (`custom_systems/es_systems.xml`):**
   - Command: `flatpak-spawn --host /home/deck/Applications/pokedaisy.AppImage --retrodeck --fullscreen "%ROM%"`.
   - Note: [`linux/CMakeLists.txt`](../../linux/CMakeLists.txt) sets `USE_LIBZIP=OFF` on mGBA, so either restrict ES-DE `<extension>` to `.gba .gbc .gb` or extract `.zip`/`.7z` in a lightweight wrapper before passing the ROM path.
2. **Tender custom runner documentation:**
   - Document configuring Tender's custom GBA runner to invoke `~/Applications/pokedaisy.AppImage --fullscreen --save-dir <tender_save_dir> "%ROM%"`.
3. **(Optional Track B) Libretro `1280x800` preset:**
   - Add `"1280x800"` to `pokedaisy_panel` in `libretro/pd_libretro.c` for users running `pokedaisy_libretro.so` directly inside RetroDECK's RetroArch.

---

### Phase 6 — AppImage Packaging & CI
**Files:** [`linux/package.py`](../../linux/package.py), [`.github/workflows/linux.yml`](../../.github/workflows/linux.yml)

1. **Add `--appimage` mode to `linux/package.py`:**
   - Assemble `PokeDaisy.AppDir/` (`usr/bin/pokedaisy-linux`, `PokeDaisy.desktop`, `linux/icon.png`, `AppRun`, `LICENSE`, `NOTICE`, `licenses/`).
   - Bundle `libSDL2-2.0.so.0` (excluding core system/GL/Wayland/X11 libraries) and package with `appimagetool` into `pokedaisy-linux-x86_64.AppImage`.
   - Include `.desktop` metadata (`Categories=Game;Emulator;`) and Steam Grid artwork for Non-Steam Shortcut setup.
2. **Update CI (`.github/workflows/linux.yml`):**
   - Build and upload `pokedaisy-linux-x86_64.AppImage` alongside the existing `.zip` artifact in the `native` job.

---

## 4. Verification Plan

1. **Unit & Host I/O Tests ([`linux/test_host_io.c`](../../linux/test_host_io.c)):**
   - Verify CLI > env > `--retrodeck` > config > XDG path resolution order, stem `.sav` naming, backup isolation, and `pocket_save_flush()`.
2. **Headless Render & Layout Tests ([`linux/test_runtime.c`](../../linux/test_runtime.c)):**
   - Run with `POCKET_WINDOW=1280x800 --layout side-by-side --shot ...` against diagnostic ROMs; verify `lastGameRect` (`960×640` ORIGINAL / `960×800` EXPAND), non-zero companion rendering in `x >= 960`, and continuous frame execution while the sidebar is visible.
3. **CI & Device Smoke Tests:**
   - Run `cmake --build linux/build && ctest --test-dir linux/build --output-on-failure` and `test_package.py`.
   - Smoke-test the `.AppImage` on SteamOS under Gamescope (`1280×800`), via RetroDECK ES-DE (`flatpak-spawn --host`), and with Tender `.sav` round-trip sync.

