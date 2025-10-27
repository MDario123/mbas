#ifndef MBAS_CONFIG_C
#define MBAS_CONFIG_C

#include "../deps/tomlc17/tomlc17.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const CONFIG_FILE_PATH = "/etc/mbas/config.toml";

enum Mode { MODE_WAV = 0 };
enum Backend { BACKEND_PIPEWIRE = 0 };

typedef enum Mode Mode;
typedef enum Backend Backend;

struct Config {
  Mode mode;
  Backend backend;

  union {
    struct {
      char *sample_path;
      char *step_seq_path;
    } wav;
  } options;
};

typedef struct Config Config;

Config load_config() {
  // Initialize empty config
  Config config;

  // Parse the config file
  toml_result_t result = toml_parse_file_ex(CONFIG_FILE_PATH);

  // Check for parsing errors
  if (!result.ok) {
    fprintf(stderr, "Error parsing config file: %s\n", result.errmsg);
    goto fail;
  }

  // Read global options
  toml_datum_t mode = toml_seek(result.toptab, "mode");
  toml_datum_t backend = toml_seek(result.toptab, "backend");

  // Mode
  if (mode.type != TOML_STRING) {
    fprintf(stderr, "Error: 'mode' must be a string in config file\n");
    goto fail;
  }

  if (strcmp(mode.u.s, "WAV") == 0) {
    config.mode = MODE_WAV;
  } else {
    fprintf(stderr, "Error: unsupported mode '%s' in config file\n", mode.u.s);
    goto fail;
  }

  // Backend
  if (backend.type != TOML_STRING) {
    fprintf(stderr, "Error: 'backend' must be a string in config file\n");
    goto fail;
  }
  if (strcmp(backend.u.s, "PIPEWIRE") == 0) {
    config.backend = BACKEND_PIPEWIRE;
  } else {
    fprintf(stderr, "Error: unsupported backend '%s' in config file\n",
            backend.u.s);
    goto fail;
  }

  switch (config.mode) {
  case MODE_WAV: {
    toml_datum_t sample_path = toml_seek(result.toptab, "wav.sample_path");
    toml_datum_t step_seq_path = toml_seek(result.toptab, "wav.step_seq_path");

    if (sample_path.type != TOML_STRING) {
      fprintf(stderr, "Error: 'wav.sample_path' must be a string\n");
      goto fail;
    }
    if (step_seq_path.type != TOML_STRING) {
      fprintf(stderr, "Error: 'wav.step_seq_path' must be a string\n");
      goto fail;
    }

    config.options.wav.sample_path = strdup(sample_path.u.s);
    config.options.wav.step_seq_path = strdup(step_seq_path.u.s);
    break;
  }
  }

  toml_free(result);
  return config;

fail:
  toml_free(result);
  exit(EXIT_FAILURE);
}

#endif
