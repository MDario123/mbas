#ifndef MBAS_CONFIG_C
#define MBAS_CONFIG_C

#include "../deps/tomlc17/tomlc17.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const CONFIG_FILE_PATH = "~/.config/mbas/config.toml";

enum Mode { MODE_SINGLE_SAMPLE = 0 };
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
    } single_sample;
  } options;
};

typedef struct Config Config;

struct load_config_result_t {
  int code;
  char *errmsg;
};

typedef struct load_config_result_t load_config_result_t;

enum {
  LOAD_CONFIG_SUCCESS = 0,
  LOAD_CONFIG_FILE_LOAD_ERROR = -1,
  LOAD_CONFIG_PARSE_ERROR = -2,
  LOAD_CONFIG_INVALID_OPTION_VALUE = -3,
};

char *expand_path(char *path) {
  char *new_path;
  if (path[0] == '~') {
    const char *home = getenv("HOME");
    size_t path_len = strlen(path);
    size_t home_len = home ? strlen(home) : 0;
    if (home) {
      new_path = malloc(home_len + path_len);
      strcpy(new_path, home);
      strcat(new_path, path + 1);
      free(path);
    } else {
      new_path = path;
    }
  }
  return new_path;
}

toml_datum_t toml_seek_typed(toml_datum_t root, const char *option_name,
                             toml_type_t exp_type, load_config_result_t *ret) {
  toml_datum_t datum = toml_seek(root, option_name);

  if (datum.type != exp_type) {
    ret->code = LOAD_CONFIG_INVALID_OPTION_VALUE;
    size_t buf_size = 128;
    ret->errmsg = (char *)malloc(buf_size);
    snprintf(ret->errmsg, buf_size,
             "Error: '%s' has invalid type in config file.", option_name);
  }

  return datum;
}

load_config_result_t load_config_file(Config *config, const char *path) {
  load_config_result_t ret = {0};
  ret.code = LOAD_CONFIG_SUCCESS;

  FILE *file = fopen(path, "r");

  if (!file) {
    ret.code = LOAD_CONFIG_FILE_LOAD_ERROR;
    ret.errmsg = strdup("Failed to open config file.");
    goto end;
  }

  // Parse the config file
  toml_result_t result = toml_parse_file(file);

  // Check for parsing errors
  if (!result.ok) {
    ret.code = LOAD_CONFIG_PARSE_ERROR;
    ret.errmsg = strdup(result.errmsg);
    goto end;
  }

  // Read global options
  toml_datum_t mode = toml_seek_typed(result.toptab, "mode", TOML_STRING, &ret);
  toml_datum_t backend =
      toml_seek_typed(result.toptab, "backend", TOML_STRING, &ret);

  if (ret.code != LOAD_CONFIG_SUCCESS) {
    goto end;
  }

  // Mode
  if (strcmp(mode.u.s, "single_sample") == 0) {
    config->mode = MODE_SINGLE_SAMPLE;
  } else {
    ret.code = LOAD_CONFIG_INVALID_OPTION_VALUE;
    ret.errmsg = strdup("Error: unsupported mode in config file. Supported "
                        "modes: \"single_sample\".");
    goto end;
  }

  // Backend
  if (strcmp(backend.u.s, "pipewire") == 0) {
    config->backend = BACKEND_PIPEWIRE;
  } else {
    ret.code = LOAD_CONFIG_INVALID_OPTION_VALUE;
    ret.errmsg =
        strdup("Error: unsupported backend in config file. Supported backends: "
               "\"pipewire\".");
    goto end;
  }

  switch (config->mode) {
  case MODE_SINGLE_SAMPLE: {
    toml_datum_t sample_path = toml_seek_typed(
        result.toptab, "single_sample.sample_path", TOML_STRING, &ret);
    toml_datum_t step_seq_path = toml_seek_typed(
        result.toptab, "single_sample.step_seq_path", TOML_STRING, &ret);

    if (ret.code != LOAD_CONFIG_SUCCESS) {
      goto end;
    }

    config->options.single_sample.sample_path = strdup(sample_path.u.s);
    config->options.single_sample.sample_path =
        expand_path(config->options.single_sample.sample_path);
    config->options.single_sample.step_seq_path = strdup(step_seq_path.u.s);
    config->options.single_sample.step_seq_path =
        expand_path(config->options.single_sample.step_seq_path);
    break;
  }
  }

end:
  toml_free(result);
  return ret;
}

load_config_result_t load_config(Config *config) {
  // TODO: look for it in the valid XDG config paths
  char *config_path = expand_path(strdup(CONFIG_FILE_PATH));
  load_config_result_t conf = load_config_file(config, config_path);
  free(config_path);
  return conf;
}

void free_config(Config *config) {
  switch (config->mode) {
  case MODE_SINGLE_SAMPLE:
    free(config->options.single_sample.sample_path);
    free(config->options.single_sample.step_seq_path);
    break;
  }
}

#endif
