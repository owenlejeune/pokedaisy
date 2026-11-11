/* Linux host CLI parsing, XDG defaults, and cascading save/state path resolution. */
#ifndef POCKET_HOST_PATHS_H
#define POCKET_HOST_PATHS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

enum pocket_layout_mode {
  POCKET_LAYOUT_AUTO = 0,
  POCKET_LAYOUT_SIDE_BY_SIDE,
  POCKET_LAYOUT_OVERLAY,
};

enum pocket_save_naming {
  POCKET_SAVE_NAMING_AUTO = 0,
  POCKET_SAVE_NAMING_STEM,
  POCKET_SAVE_NAMING_FULL,
};

enum pocket_save_ext {
  POCKET_SAVE_EXT_AUTO = 0, // .sav by default, falling back to an existing .srm
  POCKET_SAVE_EXT_SAV,      // force .sav (--save-ext sav)
  POCKET_SAVE_EXT_SRM,      // force .srm (--save-ext srm)
};

struct pocket_cli {
  const char *dataDir;
  const char *saveFile;
  const char *saveDir;
  const char *stateDir;
  const char *rom;
  const char *shot;
  int frames;
  bool fullscreen;
  bool retrodeck;
  bool showHelp;
  bool showVersion;
  enum pocket_layout_mode layout;
  enum pocket_save_naming saveNaming;
  enum pocket_save_ext saveExt;
};

struct pocket_paths {
  char configDir[1024];
  char configFile[1024];
  char dataDir[1024];
  char romsDir[1024];
  char savesDir[1024];
  char statesDir[1024];
  char backupsDir[1024];
  char controllerDb[1024];
  bool useStemNaming;
  bool isolateBackups;
  bool legacyCwd;
};

/* Parse argc/argv into cli. Returns false on unknown flags or missing option values. */
bool pocket_parse_cli(int argc, char **argv, struct pocket_cli *cli);

/* Print CLI usage to stream (typically stdout or stderr). */
void pocket_print_usage(const char *prog, void *stream);

/* Expand leading ~/ using $HOME into out (cap bytes). Returns false on truncation. */
bool pocket_expand_home(const char *in, char *out, size_t cap);

/* Recursively create dir and parents (like mkdir -p). */
bool pocket_mkdir_p(const char *dir, mode_t mode);

/* Resolve configDir, configFile, dataDir, romsDir, controllerDb (step 1 before loading settings.ini). */
bool pocket_resolve_base(const struct pocket_cli *cli, struct pocket_paths *paths);

/* Resolve savesDir, statesDir, backupsDir, and save-naming mode using cascading precedence:
 * CLI flag > env ($POKEDAISY_SAVE_DIR / $POKEDAISY_STATE_DIR) > --retrodeck > config value > fallback. */
bool pocket_resolve_storage(const struct pocket_cli *cli, struct pocket_paths *paths,
                            const char *cfgSaveDir, const char *cfgStateDir);

/* Compute battery save and save state file paths for romPath. */
bool pocket_game_save_path(const struct pocket_cli *cli, const struct pocket_paths *paths,
                           const char *romPath, char *savOut, size_t savCap,
                           char *ssOut, size_t ssCap);

#endif

