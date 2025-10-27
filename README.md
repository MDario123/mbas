# Music Box Audio Service

## Interface

Receives `PLAY` signals on `/run/mbas.sock`.

### Configuration

Reads config from `/etc/mbas/config.toml`.
[TOML specification](https://toml.io/en/v1.0.0)

#### Parameters

- `MODE`: "WAV" (planned "MIDI" support in future)
- `BACKEND`: "PIPEWIRE" (planned "ALSA" and "PULSEAUDIO" support in future)

If `MODE` is "WAV", the following parameters are used:

- `SAMPLE_PATH`: path to the WAV sample file
- `STEP_SEQ_PATH`: path to the step sequence file

Step sequence file format:

- Each line represents a step.
- Each line contains 2 integer values separated by whitespace. Representing the start and end sample indices to be played.
- Lines starting with `#` are comments and should be ignored.
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

TODO
