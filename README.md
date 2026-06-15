# nvAL

An OpenAL plugin for [NVGT](https://nvgt.dev) (NonVisual Gaming Toolkit).

Wraps OpenAL via dynamic loading — no import library required, just drop `openal32.dll` next to your game. Uses [openal-soft](https://github.com/kcat/openal-soft) headers (bundled) for types and constants only.

## Features

- **Devices & contexts** — open output devices, manage OpenAL contexts
- **Buffers & sources** — load PCM audio, control playback, gain, pitch, looping
- **3D audio** — per-source position, velocity, direction; listener orientation; distance rolloff models
- **Streaming** — buffer queue API for large files or real-time audio
- **EFX effects** — reverb, echo, chorus, flanger, compressor, equalizer via auxiliary effect slots
- **EFX filters** — lowpass, highpass, bandpass applied directly to sources
- **HRTF** — head-related transfer function via ALC_SOFT_HRTF (openal-soft)
- **Capture** — microphone input via the ALC capture extension

## Building

### Requirements

- [NVGT](https://nvgt.dev) source or SDK (for `nvgt_plugin.h` and `angelscript.h`)
- Windows: Visual Studio 2019+ Build Tools
- Linux/macOS: GCC or Clang

### Windows

```bat
cd build
build.bat C:\path\to\nvgt
```

This produces `nval.dll` in the repo root.

### Linux / macOS

```bash
cd build
./build.sh /path/to/nvgt
```

### As an NVGT plugin (SCons)

Copy or symlink this repo into `nvgt/plugin/nvAL/`, then build NVGT normally. The `_SConscript` at the repo root handles it.

## Usage

Place `nval.dll` and `openal32.dll` next to `nvgt.exe` (or your compiled game), then:

```angelscript
#pragma plugin nval

void main() {
    al_device@ dev = al_device();
    dev.open();
    al_context@ ctx = al_context(dev);
    ctx.make_current();

    // Decode any audio file to raw PCM (include helpers/al_load.nvgt for al_load()):
    //   string pcm; int fmt, sr; al_load("test.wav", pcm, fmt, sr);
    al_buffer@ buf = al_buffer(ctx);
    buf.set_data(pcm_bytes, AL_FORMAT_MONO16, 44100);

    al_source@ src = al_source(ctx);
    src.set_buffer(buf);
    src.looping = true;
    src.play();
}
```

## Examples

| File | Demonstrates |
|------|-------------|
| `basic_playback.nvgt` | Load and play a WAV file |
| `opus_playback.nvgt` | Decode and play an Ogg Opus file |
| `3d_positioning.nvgt` | Move a sound source in 3D space |
| `efx_reverb.nvgt` | Reverb via EFX auxiliary effect slot |
| `efx_echo.nvgt` | Echo effect with adjustable delay and feedback |
| `efx_lowpass.nvgt` | Lowpass direct filter (muffling effect) |
| `streaming.nvgt` | Buffer-queue streaming for large files |
| `capture.nvgt` | Microphone capture and playback |
| `hrtf.nvgt` | HRTF model selection and 3D audio |
| `device_info.nvgt` | List all devices, extensions, and capabilities |

Examples expect `test.wav` (mono recommended for 3D) or `test.opus` in the `examples/` folder.

### Audio loader

`helpers/al_load.nvgt` provides `al_load(path, pcm, fmt, sr)` — decodes any audio file (WAV, Ogg Opus, Ogg Vorbis, FLAC, MP3) using NVGT's built-in `audio_decoder`. No plugin required.

```angelscript
#include "../helpers/al_load.nvgt"
// ...
string pcm; int fmt, sr;
if (!al_load("test.wav", pcm, fmt, sr)) { alert("Error", "Could not load audio."); return; }
```

### Sound class

`helpers/al_sound.nvgt` provides `al_sound` — a wrapper compatible with NVGT's built-in `sound` class. Call `al_set_default_context(ctx)` once, then construct `al_sound` instances. It supports whole-file `load()` and queued `stream()` (call `update()` each frame while streaming). See the comments at the top of the file for the differences from NVGT's `sound`.

### Sound pools

Two pools are provided, in two deliberately different styles — pick whichever fits how you think. Both need a context; include `al_load.nvgt` (and, for the classic pool, `al_sound.nvgt`) first.

| Pool | File | Style |
|------|------|-------|
| `al_sound_pool` | `helpers/al_sound_pool.nvgt` | Lean, OpenAL-native: filename→buffer cache (decode once), recycled sources, integer handles |
| `al_sound_pool_classic` | `helpers/al_sound_pool_classic.nvgt` | The classic NVGT `sound_pool` API: `play_2d`/`play_3d`, ranges, rotation, owners/priority, `update_listener_*` |

Both distinguish **one-shots** (whole file decoded up front — best for short, frequently-repeated SFX) from **streams** (decoded on the fly into a rotating buffer queue — best for music and large files). **When anything is streaming, call the pool's `update()` once per frame** to keep the queue fed; it is a no-op when nothing is streaming, so it is always safe to call.

**`al_sound_pool`** (modern):

```angelscript
#include "../helpers/al_load.nvgt"
#include "../helpers/al_sound_pool.nvgt"
// ...
al_sound_pool pool(ctx, dev /*optional, for HRTF*/, 32 /*capacity*/);

pool.play_oneshot("step.ogg");            // cached, source recycled when done
pool.play_oneshot_3d("door.ogg", x, y, z);
int music = pool.play_stream("music.ogg", true /*looping*/);

// game loop:
pool.update();   // pumps active streams; harmless when none are
```

`play()` / `play_3d()` remain as-is (`play_oneshot` / `play_oneshot_3d` are clearer-named aliases). Streams occupy a pooled source until they finish or you `stop_sound()` them, so raise `capacity` if you run several alongside your SFX.

**`al_sound_pool_classic`** (classic `sound_pool` API):

```angelscript
#include "../helpers/al_load.nvgt"
#include "../helpers/al_sound.nvgt"
#include "../helpers/al_sound_pool_classic.nvgt"
// ...
al_set_default_context(ctx);
al_sound_pool_classic pool(ctx);

// One-shots load fully by default (classic behaviour):
pool.play_2d("step.ogg", listener_x, 0, source_x, 0, false);

// Stream a single sound via the trailing `stream` flag on any *_extended method:
pool.play_stationary_extended("music.ogg", true /*looping*/, 0, 0.0, 1.0, 100.0,
                              true /*persistent*/, null, null, true /*stream*/);

// ...or stream everything in this pool:
pool.stream_by_default = true;

// game loop:
pool.update_listener_2d(listener_x, listener_y); // also pumps streams
pool.update();                                   // needed for streams without listener updates
```

Streaming is opt-in here so short SFX stay low-latency; set `stream_by_default = true` if you'd rather stream everything.

## License

Copyright (c) 2026 tunmi13productions

Permission is granted to use, modify, and distribute this software for any purpose,
provided that the original author (tunmi13productions) is credited and the software
is not represented as your own original work.
