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

int main() {
  Config config;
  load_config_result_t res = load_config(&config);

  if (res.code != LOAD_CONFIG_SUCCESS) {
    fprintf(stderr, "Failed to load config: %s\n", res.errmsg);
    free(res.errmsg);
    return res.code;
  }

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
