#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/result.h>

#include "config.c"
#include "data.c"
#include "pipewire/stream.h"
#include "spa/param/audio/raw.h"

const char *const SOCKET_PATH = "/tmp/mbas.sock";

const char *const PLAY_COMMAND = "PLAY";

const int DEFAULT_RATE = 44100;
const int DEFAULT_CHANNELS = 1;

struct event_loop_data {
  struct pw_main_loop *loop;
  struct pw_stream *stream;

  size_t current_step;
  size_t current_step_pos;
  bool play_next;

  Data data;
};

typedef struct event_loop_data event_loop_data;

void init_event_loop_data(event_loop_data *data) {
  data->current_step = data->data.step_sequence_length;
  data->play_next = false;
}

static int stop_stream(struct spa_loop *loop, bool async, uint32_t seq,
                       const void *_data, size_t size, void *userdata) {
  event_loop_data *d = userdata;
  pw_stream_set_active(d->stream, false);

  return 0;
}

void on_process(void *userdata) {
  bool should_stop = false;
  event_loop_data *data = userdata;

  struct pw_buffer *b;
  struct spa_buffer *buf;
  uint32_t n_frames, stride, pending_frames;
  uint8_t *p;

  if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
    pw_log_warn("out of buffers: %m");
    return;
  }

  buf = b->buffer;
  if ((p = buf->datas[0].data) == NULL)
    return;

  stride = sizeof(int32_t) * DEFAULT_CHANNELS;
  n_frames = buf->datas[0].maxsize / stride;
  if (b->requested)
    n_frames = SPA_MIN((int)b->requested, n_frames);

  pending_frames = n_frames;

  if (data->current_step == data->data.step_sequence_length) {
    if (data->play_next) {
      data->current_step = 0;
      data->current_step_pos = data->data.step_sequence_l[0];
      data->play_next = false;
    } else {
      data->current_step = 0;
      data->current_step_pos = data->data.step_sequence_r[0];
    }
  }

  while (pending_frames > 0 &&
         (data->current_step_pos <
              data->data.step_sequence_r[data->current_step] ||
          data->play_next)) {
    size_t available_frames =
        data->data.step_sequence_r[data->current_step] - data->current_step_pos;
    size_t copy_frames = SPA_MIN(available_frames, pending_frames);

    memcpy(p + (n_frames - pending_frames) * stride,
           &data->data.sample[data->current_step_pos], copy_frames * stride);

    pending_frames -= copy_frames;
    data->current_step_pos += copy_frames;

    if (data->current_step_pos ==
        data->data.step_sequence_r[data->current_step]) {
      if (!data->play_next) {
        data->current_step = data->data.step_sequence_length;
        should_stop = true;
        break;
      }
      if (data->current_step == data->data.step_sequence_length - 1) {
        data->current_step = 0;
        data->current_step_pos = data->data.step_sequence_l[0];
      } else {
        data->current_step = data->current_step + 1;
        data->current_step_pos = data->data.step_sequence_l[data->current_step];
      }
      data->play_next = false;
    }
  }

  // Fill remaining with silence if needed
  if (pending_frames > 0) {
    memset(p + (n_frames - pending_frames) * stride, 0,
           pending_frames * stride);
  }

  buf->datas[0].chunk->offset = 0;
  buf->datas[0].chunk->stride = stride;
  buf->datas[0].chunk->size = n_frames * stride;

  pw_stream_queue_buffer(data->stream, b);

  if (should_stop) {
    pw_loop_invoke(pw_main_loop_get_loop(data->loop), stop_stream, 0, NULL, 0,
                   false, userdata);
  }
}

void on_msg(void *userdata, int fd, uint32_t mask) {
  event_loop_data *data = userdata;
  char buffer[256];
  ssize_t n = recv(fd, buffer, sizeof(buffer) - 1, 0);

  if (n < 0) {
    perror("recv");
    return;
  }

  buffer[n] = '\0';
  if (strncmp(buffer, PLAY_COMMAND, 4) == 0) {
    // Start playback
    printf("Received PLAY command\n");
    int res = pw_stream_set_active(data->stream, true);
    data->play_next = true;
  } else {
    fprintf(stderr, "Unknown command received: %s\n", buffer);
  }

  return;
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number) {
  event_loop_data *data = userdata;
  pw_main_loop_quit(data->loop);
}

int main() {
  // Load configuration
  Config config;
  load_config_result_t res = load_config(&config);

  if (res.code != LOAD_CONFIG_SUCCESS) {
    fprintf(stderr, "Failed to load config: %s\n", res.errmsg);
    free(res.errmsg);
    return res.code;
  }

  // Initialize data from config
  Data internal_data = data_from_config(&config);
  free_config(&config);

  // Setup UNIX domain socket server
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

  // Set socket to non-blocking
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    goto close_socket;
  }
  flags |= O_NONBLOCK;
  if (fcntl(sockfd, F_SETFL, flags) == -1) {
    perror("fcntl");
    goto close_socket;
  }

  printf("Server is listening on %s\n", SOCKET_PATH);

  // TODO: Make backend variable
  // Initialize audio backend
  event_loop_data data;
  data.data = internal_data;
  init_event_loop_data(&data);

  const struct spa_pod *params[1];
  uint8_t buffer[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

  pw_init(0, 0);

  data.loop = pw_main_loop_new(0 /* properties */);

  // Set handlers for SIGINT and SIGTERM to stop the main loop
  pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
  pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

  data.stream = pw_stream_new_simple(
      pw_main_loop_get_loop(data.loop), "Music Box Audio Service",
      pw_properties_new(
          PW_KEY_MEDIA_TYPE, "Audio",        // Clearly we want to play audio
          PW_KEY_MEDIA_CATEGORY, "Playback", // Simple playback stream
          PW_KEY_MEDIA_ROLE, "Notification", // Original purpose
          NULL                               // End of properties
          ),
      &stream_events, &data);

  /* Make one parameter with the supported formats. The SPA_PARAM_EnumFormat
   * id means that this is a format enumeration (of 1 value). */
  params[0] = spa_format_audio_raw_build(
      &b, SPA_PARAM_EnumFormat,
      &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32_LE,
                               .channels = DEFAULT_CHANNELS,
                               .rate = DEFAULT_RATE));

  /* Now connect this stream. We ask that our process function is
   * called in a realtime thread. */
  int res_con = pw_stream_connect(data.stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                                  PW_STREAM_FLAG_AUTOCONNECT |
                                      PW_STREAM_FLAG_MAP_BUFFERS |
                                      PW_STREAM_FLAG_INACTIVE,
                                  params, 1);

  if (res_con < 0) {
    fprintf(stderr, "Failed to connect stream: %s\n", spa_strerror(res_con));
    goto cleanup_backend;
  }

  // Register socket fd with the main loop
  struct spa_source *source =
      pw_loop_add_io(pw_main_loop_get_loop(data.loop), sockfd, SPA_IO_IN, false,
                     on_msg, &data);

  // Finally!!!
  // Run the main loop
  pw_main_loop_run(data.loop);

  // Cleanup
  pw_stream_destroy(data.stream);
  pw_main_loop_destroy(data.loop);
  pw_deinit();
  close(sockfd);
  return EXIT_SUCCESS;

cleanup_backend:
  pw_stream_destroy(data.stream);
  pw_main_loop_destroy(data.loop);
  pw_deinit();
close_socket:
  close(sockfd);
exit_failure:
  return EXIT_FAILURE;
}
