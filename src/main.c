#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef DEBUG
const char *const SOCKET_PATH = "/tmp/mbas.sock";
#else
const char *const SOCKET_PATH = "/run/mbas.sock";
#endif
const int MAX_CONNECTIONS = 5;

int main() {

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

    recv(sockfd, buffer, 4, 0);

    if (strncmp(buffer, "PLAY", 4) == 0) {
#ifdef DEBUG
      printf("Received PLAY command\n");
#endif
    }
  }

close_socket:
  close(sockfd);
exit_failure:
  return EXIT_FAILURE;
}
