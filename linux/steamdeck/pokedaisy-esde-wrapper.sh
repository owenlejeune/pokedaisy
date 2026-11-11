#!/usr/bin/env bash
# Optional ES-DE wrapper when launching compressed (.zip/.7z) ROMs with PokeDaisy.
# Point es_systems.xml at this script via flatpak-spawn --host if you need archive extraction.
set -euo pipefail

APPIMAGE="${POKEDAISY_APPIMAGE:-/home/deck/Applications/pokedaisy.AppImage}"
ROM="${1:?usage: pokedaisy-esde-wrapper.sh <rom-path> [extra-args...]}"
shift

TMPDIR=""
cleanup() {
  if [ -n "$TMPDIR" ] && [ -d "$TMPDIR" ]; then
    rm -rf "$TMPDIR"
  fi
}
trap cleanup EXIT

ROM_LOWER="$(printf '%s' "$ROM" | tr '[:upper:]' '[:lower:]')"
case "$ROM_LOWER" in
  *.zip|*.7z)
    TMPDIR="$(mktemp -d "${XDG_RUNTIME_DIR:-/tmp}/pokedaisy-rom.XXXXXX")"
    if command -v 7z >/dev/null 2>&1; then
      7z x -y "-o$TMPDIR" "$ROM" >/dev/null
    elif [[ "$ROM_LOWER" == *.zip ]] && command -v unzip >/dev/null 2>&1; then
      unzip -q -o "$ROM" -d "$TMPDIR"
    else
      echo "pokedaisy-esde-wrapper: need 7z or unzip to extract $ROM" >&2
      exit 1
    fi
    EXTRACTED="$(find "$TMPDIR" -type f \( -iname '*.gba' -o -iname '*.gbc' -o -iname '*.gb' \) | head -n 1)"
    if [ -z "$EXTRACTED" ]; then
      echo "pokedaisy-esde-wrapper: no .gba/.gbc/.gb ROM found in $ROM" >&2
      exit 1
    fi
    # Derive save stem from the archive basename so saves match the archive filename.
    ROM_BASE="$(basename "${ROM%.*}")"
    NAMED_ROM="$TMPDIR/$ROM_BASE.${EXTRACTED##*.}"
    if [ "$EXTRACTED" != "$NAMED_ROM" ]; then
      mv "$EXTRACTED" "$NAMED_ROM"
    fi
    ROM="$NAMED_ROM"
    ;;
esac

exec "$APPIMAGE" --retrodeck --fullscreen "$@" "$ROM"
