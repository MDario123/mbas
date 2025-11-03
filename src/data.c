#include "config.c"

struct Data {
  float *sample;
  size_t sample_length;

  size_t *step_sequence_l;
  size_t *step_sequence_r;
  size_t step_sequence_length;
};

typedef struct Data Data;

Data data_from_config_wav(const Config *config) {
  Data data = {0};

  const char *sample_path = config->options.single_sample.sample_path;
  const char *step_seq_path = config->options.single_sample.step_seq_path;

  // Load sample file
  // Expected to be raw f32le mono
  FILE *sample_file = fopen(sample_path, "rb");

  if (!sample_file) {
    fprintf(stderr, "Failed to open sample file: %s\n", sample_path);
    goto exit_failure;
  }

  fseek(sample_file, 0, SEEK_END);
  size_t sample_size = ftell(sample_file);
  rewind(sample_file);

  data.sample_length = sample_size / sizeof(float);
  data.sample = (float *)malloc(sample_size);

  size_t res =
      fread(data.sample, sizeof(float), data.sample_length, sample_file);
  if (res != data.sample_length) {
    fprintf(stderr, "Failed to read sample data from file: %s\n", sample_path);
    goto close_sample_file;
  }

  fclose(sample_file);

  // Load step sequence file
  FILE *step_seq_file = fopen(step_seq_path, "r");

  if (!step_seq_file) {
    fprintf(stderr, "Failed to open step sequence file: %s\n", step_seq_path);
    goto free_sample;
  }

  // First, count the number of steps
  // Count the amount of lines that are not empty or comments
  data.step_sequence_length = 0;
  char line[256];
  while (fgets(line, sizeof(line), step_seq_file)) {
    // Skip empty lines and comments
    if (line[0] == '\n' || line[0] == '#' || line[0] == '\r') {
      continue;
    }
    data.step_sequence_length++;
  }
  rewind(step_seq_file);
  data.step_sequence_l =
      (size_t *)malloc(data.step_sequence_length * sizeof(size_t));
  data.step_sequence_r =
      (size_t *)malloc(data.step_sequence_length * sizeof(size_t));
  size_t real_index = 0;
  size_t index = 0;
  while (fgets(line, sizeof(line), step_seq_file)) {
    real_index++;
    // Skip empty lines and comments
    if (line[0] == '\n' || line[0] == '#' || line[0] == '\r') {
      continue;
    }

    size_t step_l = 0;
    size_t step_r = 0;

    int scanned = sscanf(line, "%zu %zu", &step_l, &step_r);

    if (scanned != 2) {
      fprintf(stderr, "Invalid step sequence format in file: %s at line %zu\n",
              step_seq_path, real_index);
      goto free_step_sequence;
    }

    if (step_l > step_r || step_r > data.sample_length) {
      fprintf(stderr, "Invalid step sequence values in file: %s at line %zu\n",
              step_seq_path, real_index);
      goto free_step_sequence;
    }

    data.step_sequence_l[index] = step_l;
    data.step_sequence_r[index] = step_r;
    index++;
  }

  return data;

free_step_sequence:
  free(data.step_sequence_l);
  free(data.step_sequence_r);
free_sample:
  free(data.sample);
close_sample_file:
  fclose(sample_file);
exit_failure:
  exit(EXIT_FAILURE);
}

Data data_from_config(const Config *config) {
  switch (config->mode) {
  case MODE_SINGLE_SAMPLE: {
    return data_from_config_wav(config);
  }
  default:
    fprintf(stderr,
            "Unsupported mode in config. This should not be possible.\n");
    exit(EXIT_FAILURE);
  }
}
