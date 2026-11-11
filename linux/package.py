"""Package the Linux executable, the libretro cores it loads (--core, beside it) and their license notices, never
game data.

--portmaster lays it out as a PortMaster port instead: PokeDaisy.sh beside a pokedaisy/ folder.
--appimage assembles PokeDaisy.AppDir/ (with libSDL2-2.0.so.0, desktop entry, cores, and Steam Grid artwork)
and invokes appimagetool to produce pokedaisy-linux-<arch>.AppImage."""
from __future__ import annotations

from pathlib import Path
import argparse
import os
import shutil
import subprocess
import zipfile

FORBIDDEN_EXTS = ('.gb', '.gbc', '.gba', '.nds', '.sav', '.ss', '.srm', '.bin')
EXCLUDED_LIB_PREFIXES = (
    'libc.so', 'libm.so', 'libdl.so', 'libpthread.so', 'librt.so', 'ld-linux',
    'libGL', 'libEGL', 'libGLES', 'libvulkan', 'libX', 'libxcb', 'libwayland',
    'libxkbcommon', 'libdrm', 'libgbm', 'libdbus', 'libasound', 'libpulse',
    'libpipewire', 'libjack', 'libsndio', 'libstdc++', 'libgcc_s',
)


def parse_binary(binary: Path, dsperate: bool, cores: list[Path]) -> str:
    data = binary.read_bytes()
    header = data[:20]
    if header[:6] != b'\x7fELF\x02\x01':
        raise SystemExit('Expected a 64-bit little-endian Linux ELF')
    arch = int.from_bytes(header[18:20], 'little')
    if arch not in (62, 183):
        raise SystemExit('Supported package targets: x86_64 or AArch64')
    for core in cores:
        ch = core.read_bytes()[:20]
        if ch[:6] != header[:6] or ch[18:20] != header[18:20]:
            raise SystemExit(f'{core}: not a core for the binary\'s architecture')
    if dsperate and b'DSperate %s' not in data:
        raise SystemExit('Expected a POCKET_DSPERATE build')
    return 'aarch64' if arch == 183 else 'x86_64'


def copy_notices(repo: Path, dest: Path, dsperate: bool, cores: list[Path]) -> None:
    for source, target in [
        (repo / 'linux/README.md', dest / 'README.md'),
        (repo / 'LICENSE', dest / 'LICENSE'),
        (repo / 'NOTICE', dest / 'NOTICE'),
    ]:
        shutil.copy2(source, target)
    licenses = dest / 'licenses'
    licenses.mkdir(exist_ok=True)
    for path, license_name in [
        ('LICENSE', 'mGBA-MPL-2.0.txt'),
        ('src/third-party/blip_buf/license.txt', 'blip_buf-LGPL-2.1.txt'),
        ('src/third-party/inih/LICENSE.txt', 'inih-BSD.txt'),
    ]:
        shutil.copy2(repo / 'third_party/mgba' / path, licenses / license_name)
    (licenses / 'PixelOperator.txt').write_text('Pixel Operator by Jayvee Enaguas. CC0. See NOTICE.\n')
    gba_core = any('_gba_' in c.name for c in cores)
    if gba_core:
        # armGBA's own code is MPL 2.0; its Game Boy side (Gambatte, GPL 2.0) is not built into the core.
        shutil.copy2(repo / 'third_party/armgba/LICENSES/MPL-2.0.txt', licenses / 'armGBA-MPL-2.0.txt')
        shutil.copy2(repo / 'third_party/zlib/LICENSE', licenses / 'zlib.txt')
    ds_core = any('_ds_' in c.name for c in cores)
    if ds_core:
        tree = repo / 'third_party/dsperate-lr'
        for f in sorted(tree.rglob('*')):
            if f.is_file() and f.name.lower().startswith(('license', 'copying', 'notice')):
                dest_notice = licenses / 'dsperate-lr' / f.relative_to(tree)
                dest_notice.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, dest_notice)
    if dsperate:
        ds = repo / 'third_party/dsperate'
        for source, target in [
            ('LICENSE', 'DSperate-GPL-3.0.txt'),
            ('src/core/bios/LICENSE.freebios', 'FreeBIOS-BSD.txt'),
            ('src/core/cart/miniz/LICENSE', 'miniz-MIT.txt'),
            ('src/core/io/dsi_font/LICENSE-WenQuanYi-MicroHei.txt', 'WenQuanYi-MicroHei.txt'),
            ('src/core/io/dsi_font/LICENSE-NotoSans-OFL-1.1.txt', 'NotoSans-OFL-1.1.txt'),
        ]:
            shutil.copy2(ds / source, licenses / target)
        shutil.copy2(repo / 'linux/dsperate/README.md', dest / 'DS-EXPERIMENT.md')
    sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()
    core = subprocess.check_output(['git', 'rev-parse', 'HEAD:third_party/mgba'], cwd=repo, text=True).strip()
    source_txt = (
        'Source: https://github.com/lidor30/pokedaisy\n'
        'Fork: https://github.com/keeganwhitacre/pokedaisy-brick\n'
        f'Commit: {sha}\nmGBA: {core}\nBuild: linux/README.md at this commit\n'
    )
    if gba_core:
        agba = subprocess.check_output(['git', 'rev-parse', 'HEAD:third_party/armgba'], cwd=repo, text=True).strip()
        source_txt += f'armGBA: {agba}\n'
    if ds_core:
        ds_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD:third_party/dsperate-lr'], cwd=repo, text=True).strip()
        source_txt += f'DSperate libretro: https://github.com/thwonp/dsperate-lr ({ds_commit})\n'
    if dsperate:
        ds_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo / 'third_party/dsperate', text=True).strip()
        source_txt += (
            'EXPERIMENTAL DS backend: https://github.com/beebono/DSperate\n'
            f'DSperate: {ds_commit}\nBuild: linux/dsperate/README.md\n'
        )
    (dest / 'SOURCE.txt').write_text(source_txt)


def find_sdl2_library(binary: Path) -> Path | None:
    if not shutil.which('ldd'):
        return None
    proc = subprocess.run(['ldd', str(binary)], capture_output=True, text=True)
    if proc.returncode != 0:
        return None
    for line in proc.stdout.splitlines():
        if '=>' not in line:
            continue
        soname, rest = [part.strip() for part in line.split('=>', 1)]
        if any(soname.startswith(prefix) for prefix in EXCLUDED_LIB_PREFIXES):
            continue
        if not soname.startswith('libSDL2-2.0.so'):
            continue
        lib_token = rest.split()[0]
        if lib_token in ('not', 'found'):
            continue
        candidate = Path(lib_token)
        if candidate.exists():
            return candidate.resolve()
    return None


def validate_no_game_files(names: list[str]) -> None:
    if any(n.lower().endswith(FORBIDDEN_EXTS) for n in names):
        raise SystemExit('Refusing to package game ROM or save data')


def build_zip_package(repo: Path, binary: Path, output: Path, name: str, portmaster: bool, dsperate: bool, cores: list[Path]) -> Path:
    top = output / name
    top.mkdir()  # Refuse to overwrite an earlier package.
    dest = top / 'pokedaisy' if portmaster else top
    dest.mkdir(exist_ok=True)
    launcher = (repo / 'linux/portmaster/PokeDaisy.sh', top / 'PokeDaisy.sh') if portmaster else (repo / 'linux/launch.sh', dest / 'launch.sh')
    shutil.copy2(binary, dest / 'pokedaisy-linux')
    shutil.copy2(launcher[0], launcher[1])
    (dest / 'pokedaisy-linux').chmod(0o755)
    launcher[1].chmod(0o755)
    for c in cores:
        shutil.copy2(c, dest / c.name)
    if not portmaster:
        shutil.copy2(repo / 'linux/icon.png', dest / 'icon.png')
        (dest / 'config.json').write_text(
            '{\n  "label": "PokeDaisy",\n  "icon": "icon.png",\n  "iconsel": "icon.png",\n'
            '  "launch": "launch.sh",\n  "description": "Pokemon companion and emulator"\n}\n'
        )
    copy_notices(repo, dest, dsperate, cores)
    archive = output / f'{name}.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(top.rglob('*')):
            if f.is_file():
                z.write(f, f.relative_to(output))
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        validate_no_game_files(z.namelist())
    return archive


def build_appimage_package(
    repo: Path,
    binary: Path,
    output: Path,
    arch: str,
    name: str,
    dsperate: bool,
    appdir_only: bool,
    sdl2_lib: Path | None,
    cores: list[Path],
) -> Path:
    appdir = output / 'PokeDaisy.AppDir'
    appdir.mkdir()  # Refuse to overwrite an existing AppDir.
    usr_bin = appdir / 'usr/bin'
    usr_lib = appdir / 'usr/lib'
    usr_bin.mkdir(parents=True)
    usr_lib.mkdir(parents=True)

    target_bin = usr_bin / 'pokedaisy-linux'
    shutil.copy2(binary, target_bin)
    target_bin.chmod(0o755)
    for c in cores:
        shutil.copy2(c, usr_bin / c.name)

    sdl2_src = sdl2_lib.resolve() if sdl2_lib else find_sdl2_library(binary)
    if sdl2_src is not None:
        sdl2_dst = usr_lib / 'libSDL2-2.0.so.0'
        shutil.copy2(sdl2_src, sdl2_dst)
        sdl2_dst.chmod(0o755)
    elif not appdir_only:
        raise SystemExit('Could not locate libSDL2-2.0.so.0 via ldd; pass --sdl2-lib or use --appdir-only')

    (appdir / 'PokeDaisy.desktop').write_text(
        '[Desktop Entry]\n'
        'Type=Application\n'
        'Name=PokeDaisy\n'
        'Comment=Pokemon companion and Game Boy / GBA emulator\n'
        'Exec=pokedaisy-linux\n'
        'Icon=pokedaisy\n'
        'Categories=Game;Emulator;\n'
        'Terminal=false\n'
    )
    shutil.copy2(repo / 'linux/icon.png', appdir / 'pokedaisy.png')
    shutil.copy2(repo / 'linux/icon.png', appdir / '.DirIcon')

    apprun = appdir / 'AppRun'
    apprun.write_text(
        '#!/bin/sh\n'
        'HERE="$(dirname "$(readlink -f "$0")")"\n'
        'export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
        'exec "$HERE/usr/bin/pokedaisy-linux" "$@"\n'
    )
    apprun.chmod(0o755)

    copy_notices(repo, appdir, dsperate, cores)

    artwork_src = repo / 'linux/steamdeck/artwork'
    if artwork_src.is_dir():
        artwork_dst = appdir / 'steam-grid'
        artwork_dst.mkdir()
        for png in sorted(artwork_src.glob('*.png')):
            shutil.copy2(png, artwork_dst / png.name)

    validate_no_game_files([str(f.relative_to(appdir)) for f in appdir.rglob('*') if f.is_file()])
    if appdir_only:
        return appdir

    appimage = output / f'{name}.AppImage'
    if appimage.exists():
        raise SystemExit(f'Refusing to overwrite existing {appimage}')
    tool = os.environ.get('APPIMAGETOOL') or shutil.which('appimagetool') or shutil.which(f'appimagetool-{arch}.AppImage')
    if not tool:
        raise SystemExit('appimagetool not found in PATH or $APPIMAGETOOL')
    env = dict(os.environ, ARCH=arch, APPIMAGE_EXTRACT_AND_RUN='1')
    subprocess.run([tool, '--no-appstream', str(appdir), str(appimage)], env=env, check=True)
    return appimage


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--portmaster', action='store_true', help='a PortMaster port: PokeDaisy.sh + pokedaisy/')
    parser.add_argument('--appimage', action='store_true', help='assemble PokeDaisy.AppDir/ and build .AppImage')
    parser.add_argument('--appdir-only', action='store_true', help='assemble PokeDaisy.AppDir/ without invoking appimagetool')
    parser.add_argument('--sdl2-lib', type=Path, help='explicit path to libSDL2-2.0.so.0 for AppImage bundling')
    parser.add_argument('--dsperate', action='store_true', help='label the opt-in DS experiment and include its notices')
    parser.add_argument('--core', type=Path, action='append', default=[], help='a core to ship beside the binary (pokedaisy_gba / _gbc / _ds)')
    args = parser.parse_args()
    if args.portmaster and (args.appimage or args.appdir_only):
        parser.error('--portmaster cannot be combined with --appimage/--appdir-only')

    repo = Path(__file__).resolve().parents[1]
    arch = parse_binary(args.binary, args.dsperate, args.core)
    prefix = 'pokedaisy-portmaster-' if args.portmaster else 'pokedaisy-linux-'
    name = f'{prefix}{arch}'
    if args.dsperate:
        name += '-ds-experiment'

    args.output.mkdir(parents=True, exist_ok=True)
    if args.appimage or args.appdir_only:
        out = build_appimage_package(
            repo, args.binary, args.output, arch, name, args.dsperate, args.appdir_only, args.sdl2_lib, args.core
        )
    else:
        out = build_zip_package(repo, args.binary, args.output, name, args.portmaster, args.dsperate, args.core)
    print(out)


if __name__ == '__main__':
    main()
