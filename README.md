# Music Box Audio Service

## Interface

Receives `PLAY` signals on `/run/mbas.sock`.

### Configuration

Reads config from `~/.config/mbas/config.toml`.
[TOML specification](https://toml.io/en/v1.0.0)

#### Parameters

- `mode`: "single_sample" (planned "multi_sample" support)
- `backend`: "pipewire" (planned "pulseaudio" support)

If `mode` is "single_sample", the following parameters are used:

- `single_sample.sample_path`: path to the f32le 44100Hz mono sample file (using ffmpeg you can do something like `ffmpeg -i input.wav -f f32le -ar 44100 -ac 1 output.raw`)
- `single_sample.step_seq_path`: path to the step sequence file

Step sequence file format:

- Each line represents a step.
- Each line contains 2 integer values separated by whitespace. Representing the start and end sample indices to be played.
- Lines starting with `#` are comments and will be ignored.
- Blank lines should also be ignored.

## Behaviour

The service has three states:

- `IDLE`
- `LAST`
- `BUFF`

Initial state is `IDLE`.

On `PLAY` signal:

- `IDLE` -> `LAST`: starts playing the next step.
- `LAST` -> `BUFF`: starts buffering the next step while playing the current one.
- `BUFF` -> `BUFF`: this is a no-op.

When finished playing a step:

- `LAST` -> `IDLE`: stops playing.
- `BUFF` -> `LAST`: starts playing the buffered step.

## Building

```sh
make build
```

## TODO

- [x] Implement WAV mode with PipeWire backend.
- [ ] Read sample from more audio formats. (probably via libsndfile)
- [ ] Implement MIDI mode.
- [ ] Implement PulseAudio backend.
- [ ] Add error handling and logging.
- [ ] Read config from valid xdg paths.
- [ ] Add hot-reloading of configuration.
