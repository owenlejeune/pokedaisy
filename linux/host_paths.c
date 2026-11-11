/* GPL-3.0. Linux host CLI parsing, XDG defaults, and cascading path resolution. */
#include "host_paths.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static bool copy_str(char *dst, size_t cap, const char *src) {
  if (!dst || !cap || !src)
    return false;
  size_t len = strlen(src);
  if (len >= cap)
    return false;
  memcpy(dst, src, len + 1);
  return true;
}

static bool join_path(char *dst, size_t cap, const char *base, const char *leaf) {
  if (!dst || !cap || !base || !leaf)
    return false;
  size_t blen = strlen(base);
  while (blen > 1 && base[blen - 1] == '/')
    blen--;
  while (*leaf == '/')
    leaf++;
  size_t llen = strlen(leaf);
  bool needSlash = blen > 0 && !(blen == 1 && base[0] == '/') && llen > 0;
  size_t total = blen + (needSlash ? 1 : 0) + llen;
  if (total >= cap)
    return false;
  memcpy(dst, base, blen);
  size_t pos = blen;
  if (needSlash)
    dst[pos++] = '/';
  memcpy(dst + pos, leaf, llen);
  dst[total] = '\0';
  return true;
}

static bool non_empty(const char *s) { return s && s[0] != '\0'; }

bool pocket_expand_home(const char *in, char *out, size_t cap) {
  if (!in || !out || !cap)
    return false;
  if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
    const char *home = getenv("HOME");
    if (non_empty(home)) {
      if (in[1] == '\0')
        return copy_str(out, cap, home);
      return join_path(out, cap, home, in + 2);
    }
  }
  return copy_str(out, cap, in);
}

bool pocket_mkdir_p(const char *dir, mode_t mode) {
  if (!non_empty(dir))
    return false;
  char buf[1024];
  if (!copy_str(buf, sizeof buf, dir))
    return false;
  size_t len = strlen(buf);
  while (len > 1 && buf[len - 1] == '/')
    buf[--len] = '\0';
  for (char *p = buf + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(buf, mode) != 0 && errno != EEXIST)
        return false;
      *p = '/';
    }
  }
  if (mkdir(buf, mode) != 0) {
    if (errno != EEXIST)
      return false;
    struct stat st;
    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode))
      return false;
  }
  return true;
}

void pocket_print_usage(const char *prog, void *stream) {
  FILE *out = stream ? (FILE *)stream : stdout;
  fprintf(out,
          "Usage: %s [OPTIONS] [ROM]\n"
          "\n"
          "Options:\n"
          "  -f, --fullscreen            Launch in borderless fullscreen\n"
          "      --retrodeck             Use RetroDECK save/state directories and stem naming\n"
          "      --data <DIR>            Override data root (roms, saves, states, settings.ini)\n"
          "      --save <FILE>           Explicit battery save file path\n"
          "      --save-dir <DIR>        Directory for .sav files\n"
          "      --state-dir <DIR>       Directory for save states\n"
          "      --layout <MODE>         Layout mode: auto, side-by-side, overlay\n"
          "      --save-naming <MODE>    Save filename mode: auto, stem, full\n"
          "      --frames <N>            Run N frames and exit\n"
          "      --shot <FILE>           Capture frame PNG on exit\n"
          "  -h, --help                  Show this help and exit\n"
          "  -V, --version               Show version and exit\n",
          prog ? prog : "pokedaisy-linux");
}

static bool parse_layout(const char *val, enum pocket_layout_mode *out) {
  if (!strcmp(val, "auto"))
    *out = POCKET_LAYOUT_AUTO;
  else if (!strcmp(val, "side-by-side"))
    *out = POCKET_LAYOUT_SIDE_BY_SIDE;
  else if (!strcmp(val, "overlay"))
    *out = POCKET_LAYOUT_OVERLAY;
  else
    return false;
  return true;
}

static bool parse_save_naming(const char *val, enum pocket_save_naming *out) {
  if (!strcmp(val, "auto"))
    *out = POCKET_SAVE_NAMING_AUTO;
  else if (!strcmp(val, "stem"))
    *out = POCKET_SAVE_NAMING_STEM;
  else if (!strcmp(val, "full"))
    *out = POCKET_SAVE_NAMING_FULL;
  else
    return false;
  return true;
}

bool pocket_parse_cli(int argc, char **argv, struct pocket_cli *cli) {
  memset(cli, 0, sizeof *cli);
  bool positionalOnly = false;
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if (!positionalOnly && !strcmp(arg, "--")) {
      positionalOnly = true;
      continue;
    }
    if (positionalOnly || arg[0] != '-' || arg[1] == '\0') {
      cli->rom = arg;
      continue;
    }
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      cli->showHelp = true;
    } else if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
      cli->showVersion = true;
    } else if (!strcmp(arg, "-f") || !strcmp(arg, "--fullscreen")) {
      cli->fullscreen = true;
    } else if (!strcmp(arg, "--retrodeck")) {
      cli->retrodeck = true;
    } else if (!strcmp(arg, "--data")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", arg);
        return false;
      }
      cli->dataDir = argv[++i];
    } else if (!strcmp(arg, "--save")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", arg);
        return false;
      }
      cli->saveFile = argv[++i];
    } else if (!strcmp(arg, "--save-dir")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", arg);
        return false;
      }
      cli->saveDir = argv[++i];
    } else if (!strcmp(arg, "--state-dir")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", arg);
        return false;
      }
      cli->stateDir = argv[++i];
    } else if (!strcmp(arg, "--layout")) {
      if (i + 1 >= argc || !parse_layout(argv[i + 1], &cli->layout)) {
        fprintf(stderr, "Invalid or missing --layout (expected auto, side-by-side, overlay)\n");
        return false;
      }
      i++;
    } else if (!strcmp(arg, "--save-naming")) {
      if (i + 1 >= argc || !parse_save_naming(argv[i + 1], &cli->saveNaming)) {
        fprintf(stderr, "Invalid or missing --save-naming (expected auto, stem, full)\n");
        return false;
      }
      i++;
    } else if (!strcmp(arg, "--frames")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", arg);
        return false;
      }
      cli->frames = atoi(argv[++i]);
    } else if (!strcmp(arg, "--shot")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", arg);
        return false;
      }
      cli->shot = argv[++i];
    } else {
      fprintf(stderr, "Unknown option: %s\n", arg);
      return false;
    }
  }
  return true;
}

static bool dir_exists(const char *path) {
  struct stat st;
  return non_empty(path) && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool resolve_xdg_subdir(const char *xdgEnv, const char *homeRel, char *out, size_t cap) {
  const char *xdg = getenv(xdgEnv);
  if (non_empty(xdg)) {
    char base[1024];
    if (!pocket_expand_home(xdg, base, sizeof base))
      return false;
    return join_path(out, cap, base, "pokedaisy");
  }
  const char *home = getenv("HOME");
  if (non_empty(home))
    return join_path(out, cap, home, homeRel);
  return copy_str(out, cap, ".");
}

static bool detect_retrodeck_base(char *out, size_t cap) {
  char direct[1024], directSaves[1024], flatpak[1024], flatpakSaves[1024];
  if (!pocket_expand_home("~/retrodeck", direct, sizeof direct) ||
      !join_path(directSaves, sizeof directSaves, direct, "saves/gba") ||
      !pocket_expand_home("~/.var/app/net.retrodeck.retrodeck/data", flatpak, sizeof flatpak) ||
      !join_path(flatpakSaves, sizeof flatpakSaves, flatpak, "saves/gba"))
    return false;
  if (dir_exists(directSaves) || dir_exists(direct))
    return copy_str(out, cap, direct);
  if (dir_exists(flatpakSaves) || dir_exists(flatpak))
    return copy_str(out, cap, flatpak);
  return copy_str(out, cap, direct);
}

bool pocket_resolve_base(const struct pocket_cli *cli, struct pocket_paths *paths) {
  memset(paths, 0, sizeof *paths);
  if (non_empty(cli->dataDir)) {
    if (!pocket_expand_home(cli->dataDir, paths->dataDir, sizeof paths->dataDir))
      return false;
    if (!copy_str(paths->configDir, sizeof paths->configDir, paths->dataDir))
      return false;
  } else {
    if (!resolve_xdg_subdir("XDG_DATA_HOME", ".local/share/pokedaisy",
                            paths->dataDir, sizeof paths->dataDir))
      return false;
    if (!resolve_xdg_subdir("XDG_CONFIG_HOME", ".config/pokedaisy",
                            paths->configDir, sizeof paths->configDir))
      return false;
  }
  return join_path(paths->configFile, sizeof paths->configFile, paths->configDir, "settings.ini") &&
         join_path(paths->romsDir, sizeof paths->romsDir, paths->dataDir, "roms") &&
         join_path(paths->controllerDb, sizeof paths->controllerDb, paths->dataDir, "gamecontrollerdb.txt");
}

bool pocket_resolve_storage(const struct pocket_cli *cli, struct pocket_paths *paths,
                            const char *cfgSaveDir, const char *cfgStateDir) {
  const char *envSave = getenv("POKEDAISY_SAVE_DIR");
  const char *envState = getenv("POKEDAISY_STATE_DIR");
  char retroBase[1024] = "";
  if (cli->retrodeck && !detect_retrodeck_base(retroBase, sizeof retroBase))
    return false;

  if (non_empty(cli->saveDir)) {
    if (!pocket_expand_home(cli->saveDir, paths->savesDir, sizeof paths->savesDir))
      return false;
  } else if (non_empty(envSave)) {
    if (!pocket_expand_home(envSave, paths->savesDir, sizeof paths->savesDir))
      return false;
  } else if (cli->retrodeck) {
    if (!join_path(paths->savesDir, sizeof paths->savesDir, retroBase, "saves/gba"))
      return false;
  } else if (non_empty(cfgSaveDir)) {
    if (!pocket_expand_home(cfgSaveDir, paths->savesDir, sizeof paths->savesDir))
      return false;
  } else {
    if (!join_path(paths->savesDir, sizeof paths->savesDir, paths->dataDir, "saves"))
      return false;
  }

  if (non_empty(cli->stateDir)) {
    if (!pocket_expand_home(cli->stateDir, paths->statesDir, sizeof paths->statesDir))
      return false;
  } else if (non_empty(envState)) {
    if (!pocket_expand_home(envState, paths->statesDir, sizeof paths->statesDir))
      return false;
  } else if (cli->retrodeck) {
    if (!join_path(paths->statesDir, sizeof paths->statesDir, retroBase, "states/gba"))
      return false;
  } else if (non_empty(cfgStateDir)) {
    if (!pocket_expand_home(cfgStateDir, paths->statesDir, sizeof paths->statesDir))
      return false;
  } else {
    if (!join_path(paths->statesDir, sizeof paths->statesDir, paths->dataDir, "states"))
      return false;
  }

  bool customSaveLocation = non_empty(cli->saveFile) || non_empty(cli->saveDir) ||
                            non_empty(envSave) || cli->retrodeck || non_empty(cfgSaveDir);
  if (cli->saveNaming == POCKET_SAVE_NAMING_STEM)
    paths->useStemNaming = true;
  else if (cli->saveNaming == POCKET_SAVE_NAMING_FULL)
    paths->useStemNaming = false;
  else
    paths->useStemNaming = !non_empty(cli->dataDir) || customSaveLocation;

  paths->isolateBackups = customSaveLocation || !non_empty(cli->dataDir);
  if (paths->isolateBackups) {
    if (!join_path(paths->backupsDir, sizeof paths->backupsDir, paths->dataDir, "backups"))
      return false;
  } else {
    paths->backupsDir[0] = '\0';
  }
  return true;
}

static bool has_rom_extension(const char *ext) {
  return !strcasecmp(ext, ".gba") || !strcasecmp(ext, ".gbc") ||
         !strcasecmp(ext, ".gb") || !strcasecmp(ext, ".nds");
}

bool pocket_game_save_path(const struct pocket_cli *cli, const struct pocket_paths *paths,
                           const char *romPath, char *savOut, size_t savCap,
                           char *ssOut, size_t ssCap) {
  if (!romPath || !savOut || !savCap || !ssOut || !ssCap)
    return false;
  const char *slash = strrchr(romPath, '/');
  const char *base = slash ? slash + 1 : romPath;
  char name[512];
  if (!copy_str(name, sizeof name, base))
    return false;
  if (paths->useStemNaming) {
    char *dot = strrchr(name, '.');
    if (dot && dot != name && has_rom_extension(dot))
      *dot = '\0';
  }
  char savLeaf[520], ssLeaf[520];
  snprintf(savLeaf, sizeof savLeaf, "%s.sav", name);
  snprintf(ssLeaf, sizeof ssLeaf, "%s.ss", name);
  if (non_empty(cli->saveFile)) {
    if (!copy_str(savOut, savCap, cli->saveFile))
      return false;
  } else if (!join_path(savOut, savCap, paths->savesDir, savLeaf)) {
    return false;
  }
  return join_path(ssOut, ssCap, paths->statesDir, ssLeaf);
}

