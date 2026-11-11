/* Unit tests for Linux host CLI parsing, XDG defaults, cascading path resolution, and save naming. */
#include "host_paths.h"
#include "pd_settings.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void set_env_or_unset(const char *k, const char *v) {
  if (v)
    assert(setenv(k, v, 1) == 0);
  else
    assert(unsetenv(k) == 0);
}

static void clear_env(void) {
  unsetenv("XDG_CONFIG_HOME");
  unsetenv("XDG_DATA_HOME");
  unsetenv("POKEDAISY_SAVE_DIR");
  unsetenv("POKEDAISY_STATE_DIR");
}

static void test_cli_parsing(void) {
  struct pocket_cli cli;
  char *argv1[] = {"pokedaisy-linux", "-f", "--retrodeck", "--save-dir", "/tmp/sav",
                   "--state-dir", "/tmp/ss", "--layout", "side-by-side",
                   "--save-naming", "stem", "--frames", "12", "--shot", "out.png",
                   "/roms/emerald.gba"};
  assert(pocket_parse_cli((int)(sizeof argv1 / sizeof argv1[0]), argv1, &cli));
  assert(cli.fullscreen);
  assert(cli.retrodeck);
  assert(strcmp(cli.saveDir, "/tmp/sav") == 0);
  assert(strcmp(cli.stateDir, "/tmp/ss") == 0);
  assert(cli.layout == POCKET_LAYOUT_SIDE_BY_SIDE);
  assert(cli.saveNaming == POCKET_SAVE_NAMING_STEM);
  assert(cli.frames == 12);
  assert(strcmp(cli.shot, "out.png") == 0);
  assert(cli.rom && strcmp(cli.rom, "/roms/emerald.gba") == 0);

  char *argvHelp[] = {"pokedaisy-linux", "--help"};
  assert(pocket_parse_cli(2, argvHelp, &cli) && cli.showHelp);

  char *argvVer[] = {"pokedaisy-linux", "-V"};
  assert(pocket_parse_cli(2, argvVer, &cli) && cli.showVersion);

  char *argvBadFlag[] = {"pokedaisy-linux", "--unknown-flag"};
  assert(!pocket_parse_cli(2, argvBadFlag, &cli));

  char *argvMissingVal[] = {"pokedaisy-linux", "--save-dir"};
  assert(!pocket_parse_cli(2, argvMissingVal, &cli));

  char *argvBadLayout[] = {"pokedaisy-linux", "--layout", "bogus"};
  assert(!pocket_parse_cli(3, argvBadLayout, &cli));

  char *argvBadNaming[] = {"pokedaisy-linux", "--save-naming", "bogus"};
  assert(!pocket_parse_cli(3, argvBadNaming, &cli));

  char *argvBadExt[] = {"pokedaisy-linux", "--save-ext", "bin"};
  assert(!pocket_parse_cli(3, argvBadExt, &cli));

  char *argvSrm[] = {"pokedaisy-linux", "--save-ext", "srm", "/roms/emerald.gba"};
  assert(pocket_parse_cli(4, argvSrm, &cli) && cli.saveExt == POCKET_SAVE_EXT_SRM);
}

static void test_xdg_and_data_modes(void) {
  clear_env();
  set_env_or_unset("HOME", "/tmp/pd-home");

  struct pocket_cli cli;
  struct pocket_paths paths;
  char sav[1024], ss[1024];

  /* Standalone default: XDG under $HOME */
  char *argvStd[] = {"pokedaisy-linux", "/roms/Pokemon - Emerald Version (USA).gba"};
  assert(pocket_parse_cli(2, argvStd, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(strcmp(paths.configFile, "/tmp/pd-home/.config/pokedaisy/settings.ini") == 0);
  assert(strcmp(paths.dataDir, "/tmp/pd-home/.local/share/pokedaisy") == 0);
  assert(strcmp(paths.savesDir, "/tmp/pd-home/.local/share/pokedaisy/saves") == 0);
  assert(strcmp(paths.statesDir, "/tmp/pd-home/.local/share/pokedaisy/states") == 0);
  assert(strcmp(paths.backupsDir, "/tmp/pd-home/.local/share/pokedaisy/backups") == 0);
  assert(paths.useStemNaming);
  assert(paths.isolateBackups);
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, "/tmp/pd-home/.local/share/pokedaisy/saves/Pokemon - Emerald Version (USA).sav") == 0);
  assert(strcmp(ss, "/tmp/pd-home/.local/share/pokedaisy/states/Pokemon - Emerald Version (USA).ss") == 0);

  /* Custom XDG_CONFIG_HOME / XDG_DATA_HOME */
  set_env_or_unset("XDG_CONFIG_HOME", "/tmp/xdg-cfg");
  set_env_or_unset("XDG_DATA_HOME", "/tmp/xdg-data");
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(strcmp(paths.configFile, "/tmp/xdg-cfg/pokedaisy/settings.ini") == 0);
  assert(strcmp(paths.dataDir, "/tmp/xdg-data/pokedaisy") == 0);
  assert(strcmp(paths.savesDir, "/tmp/xdg-data/pokedaisy/saves") == 0);

  /* --data mode preserves legacy full-filename save naming and unisolated backups */
  clear_env();
  set_env_or_unset("HOME", "/tmp/pd-home");
  char *argvData[] = {"pokedaisy-linux", "--data", "/tmp/legacy-data", "diagnostic.gb"};
  assert(pocket_parse_cli(4, argvData, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(strcmp(paths.configFile, "/tmp/legacy-data/settings.ini") == 0);
  assert(!paths.useStemNaming);
  assert(!paths.isolateBackups);
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, "/tmp/legacy-data/saves/diagnostic.gb.sav") == 0);
  assert(strcmp(ss, "/tmp/legacy-data/states/diagnostic.gb.ss") == 0);
}

static void test_cascading_precedence_and_retrodeck(void) {
  clear_env();
  char home[] = "/tmp/pd-retro-XXXXXX";
  assert(mkdtemp(home));
  set_env_or_unset("HOME", home);

  /* Create Flatpak RetroDECK directory to test auto-detection when ~/retrodeck doesn't exist */
  char flatpakSaves[1024];
  snprintf(flatpakSaves, sizeof flatpakSaves, "%s/.var/app/net.retrodeck.retrodeck/data/saves/gba", home);
  assert(pocket_mkdir_p(flatpakSaves, 0755));

  struct pocket_cli cli;
  struct pocket_paths paths;
  char sav[1024], ss[1024];

  /* 4. Config save_dir beats fallback */
  char *argvBase[] = {"pokedaisy-linux", "/roms/Pokemon v1.1.GBA"};
  assert(pocket_parse_cli(2, argvBase, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, "~/cfg-saves", "~/cfg-states"));
  char expected[1200];
  snprintf(expected, sizeof expected, "%s/cfg-saves", home);
  assert(strcmp(paths.savesDir, expected) == 0);
  snprintf(expected, sizeof expected, "%s/cfg-states", home);
  assert(strcmp(paths.statesDir, expected) == 0);

  /* 3. --retrodeck beats config, and detects Flatpak path when ~/retrodeck absent */
  char *argvRetro[] = {"pokedaisy-linux", "--retrodeck", "/roms/Pokemon v1.1.GBA"};
  assert(pocket_parse_cli(3, argvRetro, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, "~/cfg-saves", "~/cfg-states"));
  assert(strcmp(paths.savesDir, flatpakSaves) == 0);
  snprintf(expected, sizeof expected, "%s/.var/app/net.retrodeck.retrodeck/data/states/gba", home);
  assert(strcmp(paths.statesDir, expected) == 0);
  assert(paths.useStemNaming);
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  snprintf(expected, sizeof expected, "%s/Pokemon v1.1.sav", flatpakSaves);
  assert(strcmp(sav, expected) == 0);

  /* If ~/retrodeck/saves/gba exists, it takes precedence over Flatpak path */
  char directSaves[1024];
  snprintf(directSaves, sizeof directSaves, "%s/retrodeck/saves/gba", home);
  assert(pocket_mkdir_p(directSaves, 0755));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(strcmp(paths.savesDir, directSaves) == 0);

  /* 2. $POKEDAISY_SAVE_DIR beats --retrodeck */
  set_env_or_unset("POKEDAISY_SAVE_DIR", "~/env-saves");
  set_env_or_unset("POKEDAISY_STATE_DIR", "/tmp/env-states");
  assert(pocket_resolve_storage(&cli, &paths, "~/cfg-saves", "~/cfg-states"));
  snprintf(expected, sizeof expected, "%s/env-saves", home);
  assert(strcmp(paths.savesDir, expected) == 0);
  assert(strcmp(paths.statesDir, "/tmp/env-states") == 0);

  /* 1. Explicit CLI --save-dir / --state-dir beats everything */
  char *argvCli[] = {"pokedaisy-linux", "--retrodeck", "--save-dir", "/tmp/cli-saves",
                     "--state-dir", "/tmp/cli-states", "/roms/test.gba"};
  assert(pocket_parse_cli(7, argvCli, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, "~/cfg-saves", "~/cfg-states"));
  assert(strcmp(paths.savesDir, "/tmp/cli-saves") == 0);
  assert(strcmp(paths.statesDir, "/tmp/cli-states") == 0);

  /* Explicit --save <file> overrides savOut path */
  char *argvSaveFile[] = {"pokedaisy-linux", "--save", "/tmp/explicit.sav", "/roms/test.gba"};
  assert(pocket_parse_cli(4, argvSaveFile, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, "/tmp/explicit.sav") == 0);

  char cmd[1200];
  snprintf(cmd, sizeof cmd, "rm -rf %s", home);
  assert(system(cmd) == 0);
}

static void test_settings_preserves_paths(void) {
  char path[] = "/tmp/pd-settings-paths-XXXXXX";
  int fd = mkstemp(path);
  assert(fd >= 0);
  FILE *f = fdopen(fd, "w");
  assert(f);
  fputs("aspect=1\nsave_dir=/home/deck/saves/gba\nstate_dir=/home/deck/states/gba\n", f);
  fclose(f);

  struct pd_settings s;
  assert(pd_settings_load(&s, path));
  assert(s.aspect == PD_ASPECT_EXPAND);
  assert(strcmp(s.saveDir, "/home/deck/saves/gba") == 0);
  assert(strcmp(s.stateDir, "/home/deck/states/gba") == 0);

  s.hud = false;
  assert(pd_settings_save(&s, path));

  struct pd_settings reloaded;
  assert(pd_settings_load(&reloaded, path));
  assert(!reloaded.hud);
  assert(strcmp(reloaded.saveDir, "/home/deck/saves/gba") == 0);
  assert(strcmp(reloaded.stateDir, "/home/deck/states/gba") == 0);
  unlink(path);
}

static void test_legacy_cwd_fallback(void) {
  char orig[1024];
  assert(getcwd(orig, sizeof orig) != NULL);

  char tmpl[] = "/tmp/pd-legacy-cwd-XXXXXX";
  char *dir = mkdtemp(tmpl);
  assert(dir);
  assert(chdir(dir) == 0);
  assert(pocket_mkdir_p("saves", 0755));

  struct pocket_cli cli;
  struct pocket_paths paths;
  char *argv[] = {"pokedaisy-linux", "/roms/Emerald.gba"};
  assert(pocket_parse_cli(2, argv, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(paths.legacyCwd);
  assert(strcmp(paths.dataDir, ".") == 0);
  assert(strcmp(paths.configFile, "./settings.ini") == 0);
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(!paths.useStemNaming);
  assert(!paths.isolateBackups);

  char sav[1024], ss[1024];
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, "./saves/Emerald.gba.sav") == 0);
  assert(strcmp(ss, "./states/Emerald.gba.ss") == 0);

  assert(chdir(orig) == 0);
  char cmd[1200];
  snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
  assert(system(cmd) == 0);
}

static void test_save_ext_and_srm_fallback(void) {
  clear_env();
  char tmpl[] = "/tmp/pd-save-ext-XXXXXX";
  char *dir = mkdtemp(tmpl);
  assert(dir);

  char saveDir[1024], stateDir[1024], srmFile[1200], savFile[1200], sav[1024], ss[1024];
  snprintf(saveDir, sizeof saveDir, "%s/saves", dir);
  snprintf(stateDir, sizeof stateDir, "%s/states", dir);
  assert(pocket_mkdir_p(saveDir, 0755));
  assert(pocket_mkdir_p(stateDir, 0755));

  struct pocket_cli cli;
  struct pocket_paths paths;

  /* 1. Default without any existing files -> .sav */
  char *argvAuto[] = {"pokedaisy-linux", "--save-dir", saveDir, "--state-dir", stateDir, "/roms/Emerald.gba"};
  assert(pocket_parse_cli(6, argvAuto, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  snprintf(savFile, sizeof savFile, "%s/Emerald.sav", saveDir);
  assert(strcmp(sav, savFile) == 0);

  /* 2. Existing .srm triggers auto fallback when .sav is absent */
  snprintf(srmFile, sizeof srmFile, "%s/Emerald.srm", saveDir);
  FILE *f = fopen(srmFile, "wb");
  assert(f && !fclose(f));
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, srmFile) == 0);

  /* 3. Explicit --save-ext sav overrides existing .srm */
  char *argvForceSav[] = {"pokedaisy-linux", "--save-dir", saveDir, "--save-ext", "sav", "/roms/Emerald.gba"};
  assert(pocket_parse_cli(6, argvForceSav, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, savFile) == 0);

  /* 4. Both .sav and .srm present -> auto still prefers .sav */
  f = fopen(savFile, "wb");
  assert(f && !fclose(f));
  assert(pocket_parse_cli(6, argvAuto, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, savFile) == 0);

  /* 5. Explicit --save-ext srm chooses .srm even when .sav exists */
  char *argvForceSrm[] = {"pokedaisy-linux", "--save-dir", saveDir, "--save-ext", "srm", "/roms/Emerald.gba"};
  assert(pocket_parse_cli(6, argvForceSrm, &cli));
  assert(pocket_resolve_base(&cli, &paths));
  assert(pocket_resolve_storage(&cli, &paths, NULL, NULL));
  assert(pocket_game_save_path(&cli, &paths, cli.rom, sav, sizeof sav, ss, sizeof ss));
  assert(strcmp(sav, srmFile) == 0);

  char cmd[1200];
  snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
  assert(system(cmd) == 0);
}

int main(void) {
  test_cli_parsing();
  test_xdg_and_data_modes();
  test_legacy_cwd_fallback();
  test_cascading_precedence_and_retrodeck();
  test_settings_preserves_paths();
  test_save_ext_and_srm_fallback();
  puts("PASS Linux CLI, XDG defaults, legacy CWD fallback, cascading save/state resolution, stem naming, .srm fallback, and settings preservation");
  return 0;
}
