# Steam Deck PokeDaisy Testing Guide (AppImage, 2-Pane UI, RetroDECK & Tender)

Complete step-by-step guide to run and verify all PokeDaisy features on the Steam Deck in **Desktop Mode** and **Gaming Mode**.

---

## 1. Install on Steam Deck

Download the latest `pokedaisy-linux-x86_64` zip from the GitHub Actions **Linux Foundation** workflow into `~/Downloads`.

Open **Konsole** in Desktop Mode:

```bash
mkdir -p ~/Applications
unzip -j ~/Downloads/pokedaisy-linux-x86_64.zip '*x86_64.AppImage' -d ~/Applications/
mv ~/Applications/pokedaisy-linux-x86_64.AppImage ~/Applications/pokedaisy.AppImage
chmod +x ~/Applications/pokedaisy.AppImage
```

### Option A: Run as AppImage (With FUSE Bypass)
SteamOS can hit FUSE mount limits (`fusermount3: too many fuse filesystems mounted`). Prefix commands with `APPIMAGE_EXTRACT_AND_RUN=1`:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ~/Applications/pokedaisy.AppImage --help
```

### Option B: Extract Once (Recommended for SteamOS, Zero FUSE)
Extracting avoids FUSE completely, boots faster, and is immune to mount limits:

```bash
cd ~/Applications
./pokedaisy.AppImage --appimage-extract
mv squashfs-root pokedaisy
chmod +x ~/Applications/pokedaisy/AppRun
```

Then test directly:
```bash
~/Applications/pokedaisy/AppRun --help
```
*(Artwork sits in `~/Applications/pokedaisy/steam-grid/`: `grid-vertical.png`, `grid-horizontal.png`, `hero.png`, `logo.png`.)*

---

## 2. Test 2-Pane Side-by-Side UI & Controls

Launch a ROM in Desktop Mode (or add to Steam and test in Gaming Mode):

```bash
# If using AppImage:
APPIMAGE_EXTRACT_AND_RUN=1 ~/Applications/pokedaisy.AppImage --fullscreen "/path/to/game.gba"

# If using extracted folder:
~/Applications/pokedaisy/AppRun --fullscreen "/path/to/game.gba"
```

### What to Verify:

1. **Dual-Pane Viewport on 1280×800 (`AUTO` layout):**
   - **Left pane (`960×800`):** Game renders at 4× integer scale (`960×640` in `ORIGINAL` aspect, or `960×800` filling 100% height in `EXPAND` aspect).
   - **Right pane (`320×800` at `x = 960`):** Live Companion sidebar stays continuously visible without pausing emulation.
   - **In-Game HUD:** Sits inside the left game pane at whole scale.
   - **Quick Menu (`Y`):** Rings/grids overlay only the left game pane, leaving sidebar visible.

2. **Touchscreen & Trackpad:**
   - Tap/drag directly on the right sidebar (`x >= 960` on screen): tabs switch and buttons press live while character/music keeps running in the game.

3. **Controller Sidebar Focus (`focusSidebar`):**
   - Press **`X`** (or `Tab` / Deck `L4`): toggles controller focus cursor between game and sidebar.
   - When focused on sidebar: D-pad / A / B control sidebar menus; emulation continues running in the background.
   - Press **`B`** at top level of sidebar: returns focus to game.

4. **Configuring Layout (`AUTO` vs `SIDE BY SIDE` vs `OVERLAY`):**
   Test switching layout across all 3 methods:
   - **Method 1 (Host Menu):** Press `Select + Start` → cycle **`LAYOUT`** (`AUTO` → `SIDE BY SIDE` → `OVERLAY`). Select `OVERLAY` to test full-screen overlay mode; select `AUTO` or `SIDE BY SIDE` to return to 2-pane view. Saved in `~/.config/pokedaisy/host.ini`.
   - **Method 2 (Companion Menu):** Press `X` to focus Companion → navigate to **SETTINGS** tab → cycle **`LAYOUT`** (`AUTO` / `SIDE BY SIDE` / `OVERLAY`). Saved in `~/.config/pokedaisy/settings.ini`.
   - **Method 3 (CLI Flag):** Launch with `--layout overlay` (forces single-pane overlay) or `--layout side-by-side` (forces 2-pane).

5. **Host Menu & Controls (`Select + Start`):**
   - `Select + Start`: Opens host menu (`RESUME`, `SAVE STATE`, `LOAD STATE`, `FF SPEED`, `REWIND`, `LCD FILTER`, `A/B BUTTONS`, `LAYOUT`, `HOTKEYS`, `QUIT`).
   - `A/B BUTTONS`: Toggle between `NORMAL` (Nintendo East=A) and `SWAPPED` (Xbox South=A).
   - Back Paddles (default behavior when not remapped in `HOTKEYS`):
     - `L4` (`PADDLE 2`): Toggle Companion sidebar focus (`X`)
     - `R4` (`PADDLE 1`): Fast-forward (hold)
     - `R5` (`PADDLE 3`): Quick Menu (`Y`)
     - `L5` (`PADDLE 4`): Save State (`Select + L5` loads state)

---

## 3. Steam Gaming Mode Setup (Non-Steam Game)

1. Open Steam Desktop Client → **Games** → **Add a Non-Steam Game to My Library...**
2. Browse to:
   - **AppImage:** `/home/deck/Applications/pokedaisy.AppImage`
   - **Extracted:** `/home/deck/Applications/pokedaisy/AppRun` (change file filter to *All Files*).
3. Right-click shortcut in Steam → **Properties**:
   - **Target:** `/home/deck/Applications/pokedaisy/AppRun` (or `/home/deck/Applications/pokedaisy.AppImage`)
   - **Launch Options (if using AppImage):**
     ```text
     APPIMAGE_EXTRACT_AND_RUN=1 %command% --fullscreen
     ```
   - **Launch Options (if using extracted AppRun):**
     ```text
     %command% --fullscreen
     ```
     *(Add `"<path-to-rom>"` to boot directly into a game, or leave blank to open built-in ROM browser).*
4. Set Steam Grid artwork:
   - Right-click shortcut icon / banner in Steam to set custom artwork from `~/Applications/pokedaisy/steam-grid/`:
     - Vertical capsule: `grid-vertical.png`
     - Horizontal banner: `grid-horizontal.png`
     - Background hero: `hero.png`
     - Logo: `logo.png`
5. Switch to **Gaming Mode** on Steam Deck, launch, and test gamepad controls & touchscreen.

---

## 4. Test RetroDECK (ES-DE Custom System)

RetroDECK runs in a Flatpak container. ES-DE executes host commands via `flatpak-spawn --host`.

### 1. Copy ES-DE Custom System
Copy `linux/steamdeck/es_systems.xml` into RetroDECK:

```bash
mkdir -p ~/retrodeck/ES-DE/custom_systems
cp /path/to/pokedaisy/linux/steamdeck/es_systems.xml ~/retrodeck/ES-DE/custom_systems/es_systems.xml
```
*(If RetroDECK config sits in Flatpak storage, use `~/.var/app/net.retrodeck.retrodeck/config/ES-DE/custom_systems/es_systems.xml` instead).*

### 2. Compressed ROMs (`.zip` / `.7z`) Wrapper
Because mGBA has `USE_LIBZIP=OFF`, use `pokedaisy-esde-wrapper.sh` for zipped libraries:

```bash
cp /path/to/pokedaisy/linux/steamdeck/pokedaisy-esde-wrapper.sh ~/Applications/
chmod +x ~/Applications/pokedaisy-esde-wrapper.sh
```

In `es_systems.xml`, point `<command>` to the wrapper:
```xml
<command label="PokeDaisy (Standalone)">flatpak-spawn --host /home/deck/Applications/pokedaisy-esde-wrapper.sh "%ROM%"</command>
```
*(The wrapper automatically sets `APPIMAGE_EXTRACT_AND_RUN=1` and falls back to `~/Applications/pokedaisy/AppRun` if extracted).*

### 3. Verify in RetroDECK:
- Launch RetroDECK → highlight GBA game → **Edit Game Metadata** → **Alternative Emulator** → select **PokeDaisy (Standalone)**.
- Play, save in-game, and exit.
- Verify:
  - Save written to `~/retrodeck/saves/gba/<Stem>.sav`.
  - Save states written to `~/retrodeck/states/gba/<Stem>.ss`.
  - Backup files stay isolated in `~/.local/share/pokedaisy/backups/` and do **not** clutter RetroDECK saves.

---

## 5. Test Tender Bidirectional Save Sync

Point PokeDaisy at Tender's save directory to verify instant save synchronization.

### 1. Launch with Tender Directory:
```bash
~/Applications/pokedaisy/AppRun --fullscreen --save-dir "/home/deck/Tender/saves" "/path/to/Emerald.gba"
```
*(Or add `save_dir=/home/deck/Tender/saves` to `~/.config/pokedaisy/settings.ini` or set `POKEDAISY_SAVE_DIR`).*

### 2. Verify Flush-on-Write:
- In-game, save to battery SRAM.
- PokeDaisy checks dirty SRAM every ~0.5s (30 frames) and immediately upon opening the pause menu (`Select + Start`), writing atomically with `fsync()`.
- Check `/home/deck/Tender/saves/<Stem>.sav` timestamp updates immediately without quitting the game.
- Confirm Tender detects the file update via `inotify` and triggers upload.

### 3. Verify `.srm` Fallback:
- If a game has an existing `<Stem>.srm` in Tender's folder and no `.sav`, PokeDaisy automatically opens and updates `<Stem>.srm`.
- Or pass `--save-ext srm` to force `.srm` mode.

---

## 6. Troubleshooting

- **Error: `/usr/bin/fusermount3: too many fuse filesystems mounted`**
  - Use `APPIMAGE_EXTRACT_AND_RUN=1 ~/Applications/pokedaisy.AppImage ...`
  - Or extract the AppImage once with `./pokedaisy.AppImage --appimage-extract` and execute `~/Applications/pokedaisy/AppRun`.
- **Error: `open dir error: No such file or directory`**
  - Check extract flag syntax: use `--appimage-extract` (no hyphen between `app` and `image`).
- **Controller buttons swapped (A/B or X/Y reversed):**
  - Open host menu with `Select + Start` → change **`A/B BUTTONS`** between `NORMAL` (Nintendo) and `SWAPPED` (Xbox).
- **Audio stutter under battery saver / 40Hz cap:**
  - In Steam Quick Access Menu (`...`), set refresh rate to 60Hz. PokeDaisy audio trim synchronizes dynamically with 60Hz/Game Boy 59.7Hz refresh.
