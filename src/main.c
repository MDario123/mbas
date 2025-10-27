#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "config.c"

#ifdef DEBUG
const char *const SOCKET_PATH = "/tmp/mbas.sock";
#else
const char *const SOCKET_PATH = "/run/mbas.sock";
#endif

const int MAX_CONNECTIONS = 5;
const char *const PLAY_COMMAND = "PLAY";

struct Data {
  int16_t *sample;
  size_t sample_length;

  size_t *step_sequence_l;
  size_t *step_sequence_r;
  size_t step_sequence_length;
};

typedef struct Data Data;

Data data_from_config_wav(const Config *config) {
  Data data = {0};

  const char *sample_path = config->options.wav.sample_path;
  const char *step_seq_path = config->options.wav.step_seq_path;

  // Load sample file
  // Expected to be raw LPCM 16-bit little-endian mono
  FILE *sample_file = fopen(sample_path, "rb");

  if (!sample_file) {
    fprintf(stderr, "Failed to open sample file: %s\n", sample_path);
    goto exit_failure;
  }

  fseek(sample_file, 0, SEEK_END);
  size_t sample_size = ftell(sample_file);
  rewind(sample_file);

  data.sample_length = sample_size / sizeof(int16_t);
  data.sample = (int16_t *)malloc(sample_size);

  unsigned long res =
      fread(data.sample, sizeof(int16_t), data.sample_length, sample_file);
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
  case MODE_WAV: {
    return data_from_config_wav(config);
  }
  default:
    fprintf(stderr,
            "Unsupported mode in config. This should not be possible.\n");
    exit(EXIT_FAILURE);
  }
}

int main() {
  Config config;
  load_config_result_t res = load_config(&config);

  if (res.code != LOAD_CONFIG_SUCCESS) {
    fprintf(stderr, "Failed to load config: %s\n", res.errmsg);
    free(res.errmsg);
    return res.code;
  }

  Data data = data_from_config(&config);

  int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);

  if (sockfd < 0) {
    perror("socket");
    goto exit_failure;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(struct sockaddr_un));

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, SOCKET_PATH);

  unlink(SOCKET_PATH);

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    goto close_socket;
  }

  printf("Server is listening on %s\n", SOCKET_PATH);

  struct pollfd pfd = {sockfd, POLLIN, 0};

  char buffer[8];

  while (1) {
    int poll_count = poll(&pfd, 1, -1);

    if (poll_count < 0) {
      perror("poll");
      goto close_socket;
    }

    ssize_t read = recv(sockfd, buffer, 4, 0);

    if (read < 0) {
      perror("recv");
      goto close_socket;
    }

    buffer[read] = '\0';

    if (strcmp(buffer, PLAY_COMMAND) == 0) {
#ifdef DEBUG
      printf("Received %s command\n", PLAY_COMMAND);
#endif
      printf("Sample Length: %zu\n", data.sample_length);
    } else {
      fprintf(stderr, "Received unknown command: %s", buffer);
    }
  }

close_socket:
  close(sockfd);
exit_failure:
  free_config(&config);
  return EXIT_FAILURE;
}
