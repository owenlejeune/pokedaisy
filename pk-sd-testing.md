Analyzed README.md:L1-63
Analyzed es_systems.xml:L1-22
Analyzed pokedaisy-esde-wrapper.sh:L1-45
Analyzed linux.yml:L1-25
Analyzed main.c:L380-460

Follow steps in Steam Deck **Desktop Mode** (Konsole), then test in **Gaming Mode**.

### 1. Grab CI Build & Install AppImage
1. GitHub repo **Actions** → **Linux foundation** → latest run → download `pokedaisy-linux-x86_64` artifact zip.
2. Extract zip; move AppImage into `~/Applications`:
   ```bash
   mkdir -p ~/Applications
   unzip -j ~/Downloads/pokedaisy-linux-x86_64.zip '*x86_64.AppImage' -d ~/Applications/
   mv ~/Applications/pokedaisy-linux-x86_64.AppImage ~/Applications/pokedaisy.AppImage
   chmod +x ~/Applications/pokedaisy.AppImage
   ```
3. *(Optional)* Smoke-test CLI flags & extract Steam Grid art from AppImage:
   ```bash
   ~/Applications/pokedaisy.AppImage --help
   ~/Applications/pokedaisy.AppImage --appimage-extract
   # Artwork sits in squashfs-root/steam-grid/ (grid-vertical.png, grid-horizontal.png, hero.png, logo.png)
   ```

---

### 2. Test Standalone & Gaming Mode (`1280x800`, Touch, Controls)
1. **Desktop Mode quick check:**
   ```bash
   ~/Applications/pokedaisy.AppImage --fullscreen "/path/to/Emerald.gba"
   ```
   - Verify 1280×800 borderless window with game scaled on left and live companion sidebar on right (driven by core `pokedaisy_panel`).
   - Test touchscreen taps/drags directly on companion tabs & battle buttons (checks `SDL_FINGER*` + `SDL_TOUCH_MOUSEID` dedup).
2. **Gaming Mode:**
   - Steam → **Add Non-Steam Game** → `/home/deck/Applications/pokedaisy.AppImage`.
   - Set optional Steam Grid PNGs from `squashfs-root/steam-grid/` (or [artwork/](file:///Users/owen.lejeune/Documents/dev/pokedaisy/linux/steamdeck/artwork)).
   - Launch Options: `--fullscreen` (plus `"<rom-path>"` if bypassing built-in ROM picker; `gamescope` also triggers fullscreen automatically).
   - Use **Gamepad** Steam Input layout (map Steam back paddles `L4`/`R4`/`L5`/`R5` if testing controller `P1`–`P4`).
   - Verify:
     - `Select + Start` (or Guide button): opens host pause menu (`RESUME`, `SAVE STATE`, `LOAD STATE`, `HOTKEYS`, `REWIND`, `FF SPEED`, `LCD FILTER`, `A/B BUTTONS`, `QUIT`).
     - Test `A/B BUTTONS` toggle (`NORMAL` vs `SWAPPED`, saved to `~/.config/pokedaisy/host.ini`).
     - Test `HOTKEYS` assignment (`FF` default `R2`, `REWIND` default `L2`, or bind `L4`/`R4`/`L5`/`R5` once passed through by Steam Input).
     - Test `X` (companion toggle), `Y` (quick menu), and verify on-screen keyboard does not pop up unexpectedly.

---

### 3. Test RetroDECK (ES-DE Custom System)
RetroDECK runs inside Flatpak, so ES-DE launches host AppImage via `flatpak-spawn --host`.

1. **Uncompressed `.gba` ROMs:** Copy [es_systems.xml](file:///Users/owen.lejeune/Documents/dev/pokedaisy/linux/steamdeck/es_systems.xml) to your RetroDECK custom systems path (or merge `<system>` if you already have one):
   ```bash
   mkdir -p ~/retrodeck/ES-DE/custom_systems
   # Copy repo file linux/steamdeck/es_systems.xml here
   ```
   *(If your RetroDECK config sits inside Flatpak var dir instead, use `~/.var/app/net.retrodeck.retrodeck/config/ES-DE/custom_systems/es_systems.xml`.)*
   Exact launch command used:
   ```text
   flatpak-spawn --host /home/deck/Applications/pokedaisy.AppImage --retrodeck --fullscreen "%ROM%"
   ```
2. **If your ROMs are `.zip`/`.7z`:** Copy [pokedaisy-esde-wrapper.sh](file:///Users/owen.lejeune/Documents/dev/pokedaisy/linux/steamdeck/pokedaisy-esde-wrapper.sh) and update `<extension>` + `<command>` in `es_systems.xml`:
   ```bash
   chmod +x ~/Applications/pokedaisy-esde-wrapper.sh
   ```
   ```xml
   <extension>.gba .GBA .zip .ZIP .7z .7Z</extension>
   <command label="PokeDaisy (Standalone)">flatpak-spawn --host /home/deck/Applications/pokedaisy-esde-wrapper.sh "%ROM%"</command>
   ```
3. **For GB / GBC / NDS under RetroDECK:** Add matching `<system>` blocks passing explicit dirs so saves don't land in `saves/gba`:
   ```text
   flatpak-spawn --host /home/deck/Applications/pokedaisy.AppImage --retrodeck --save-dir /home/deck/retrodeck/saves/gbc --state-dir /home/deck/retrodeck/states/gbc --fullscreen "%ROM%"
   ```
4. **Verify in RetroDECK:**
   - Open RetroDECK → ES-DE → GBA game → **Edit Game Metadata / Alternative Emulator** → pick **PokeDaisy (Standalone)**.
   - Save in-game, exit.
   - Check save written as stem filename in `~/retrodeck/saves/gba/<GameStem>.sav` (or `~/.var/app/net.retrodeck.retrodeck/data/saves/gba/`).
   - Check states go to `~/retrodeck/states/gba/`.
   - Check startup save backups stay isolated in `~/.local/share/pokedaisy/backups/` (not polluting `retrodeck/saves/gba/`).

*(Alternative: test libretro cores directly inside RetroDECK RetroArch using `pokedaisy-libretro-x86_64.zip` cores `pokedaisy_gba_libretro.so`, `pokedaisy_gbc_libretro.so`, `pokedaisy_ds_libretro.so` and setting Core Option **Panel size (`pokedaisy_panel`)** = `1280x800`.)*

---

### 4. Test Tender Bidirectional Save Sync
Point PokeDaisy at Tender's managed save folder so both share `.sav` / `.srm` files while startup backups stay isolated.

1. **Configure Tender runner command (pick one method):**
   - **CLI flag (`.sav`):**
     ```bash
     ~/Applications/pokedaisy.AppImage --fullscreen --save-dir "/path/to/tender/saves" "/path/to/Emerald.gba"
     ```
   - **Force `.srm` (if Tender expects `.srm` for new saves):** add `--save-ext srm`. *(Without `--save-ext`, PokeDaisy uses `.sav`, or auto-opens an existing `<GameStem>.srm` if `.sav` is absent.)*
   - **Or persist without changing launch args:** add to `~/.config/pokedaisy/settings.ini`:
     ```ini
     save_dir=/path/to/tender/saves
     ```
     (or set env `POKEDAISY_SAVE_DIR="/path/to/tender/saves"`).
2. **Test Deck → Tender sync:**
   - Boot game via Tender / command above, advance 1 step, save in-game.
   - Within ~2 seconds (`sram_write` writes atomically + `fsync` every 120 frames, or immediately on Save State / Quit), check `/path/to/tender/saves/<GameStem>.sav` (or `.srm`) updated.
   - Confirm Tender detects file change and uploads without needing game exit.
3. **Test Tender → Deck sync + backup safety:**
   - Make change on second device (or edit save via Tender pull), sync down to `/path/to/tender/saves/<GameStem>.sav`.
   - Relaunch PokeDaisy → verify synced save loads cleanly.
   - Check backup snapshot saved in `~/.local/share/pokedaisy/backups/` (and zero `.backup-*` files inside `/path/to/tender/saves/`).
