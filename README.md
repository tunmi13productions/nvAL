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

## Prebuilt binaries

You don't need to build nvAL yourself. Every [release](https://github.com/tunmi13productions/nvAL/releases) ships all three platform plugins, built by CI:

| Platform | File | Runtime OpenAL library to ship alongside |
|----------|------|------------------------------------------|
| Windows | `nval.dll` | `openal32.dll` |
| Linux | `nval.so` | `libopenal.so.1` |
| macOS (arm64) | `nval.dylib` | `libopenal.dylib` |

Drop the plugin for your platform next to `nvgt.exe` (or your compiled game). OpenAL is loaded dynamically at runtime, so there is no import library — just make sure the OpenAL runtime library above is present.

## Building

### Requirements

- A checkout of the [NVGT **source** repo](https://github.com/samtupy/nvgt) (not an installed copy) — provides `src/nvgt_plugin.h` and `ASAddon/` (`scriptarray.h` + the plugin `scriptarray.cpp`).
- `angelscript.h` — the only external header. It is **not** committed to the NVGT repo; it ships in the platform dev bundle, downloadable from [nvgt.dev](https://nvgt.dev): [`windev.zip`](https://nvgt.dev/windev.zip) / [`lindev.zip`](https://nvgt.dev/lindev.zip) / [`macosdev.zip`](https://nvgt.dev/macosdev.zip). Extract the matching one in the NVGT repo root so you have e.g. `nvgt/windev/include/angelscript.h`.
- Windows: Visual Studio 2019+ Build Tools. Linux/macOS: GCC or Clang.

OpenAL is `dlopen`/`LoadLibrary`'d at runtime, so **no OpenAL SDK or import library is needed to build** — the bundled openal-soft headers under `src/AL` supply the types and constants.

### Windows

```bat
cd build
build.bat C:\path\to\nvgt
```

Produces `nval.dll` in the repo root.

### Linux / macOS

```bash
cd build
./build.sh /path/to/nvgt
```

Produces `nval.so` (Linux) or `nval.dylib` (macOS), detected from `uname`, in the repo root.

To point at an `angelscript.h` that isn't inside the dev bundle, set `INC_AS` to a directory containing it (both scripts honor this):

```bash
INC_AS=/path/with/angelscript.h ./build.sh /path/to/nvgt
```

### As an NVGT plugin (SCons)

Copy or symlink this repo into `nvgt/plugin/nvAL/`, then build NVGT normally. The `_SConscript` at the repo root handles it.

## Continuous integration

[`.github/workflows/build.yml`](.github/workflows/build.yml) builds all three platforms on native GitHub-hosted runners (Windows, Linux, macOS). It checks out the NVGT source, pulls just `angelscript.h` out of the matching dev bundle (cached), runs the build script above, and uploads each binary as a run artifact.

- **Every push to `main` / pull request** rebuilds all three; download them from the run's artifacts (`gh run download --repo tunmi13productions/nvAL`).
- **Pushing a tag** (e.g. `git push origin v0.1.0`) additionally publishes a GitHub Release with all three binaries attached.

The `NVGT_REF` variable at the top of the workflow pins which NVGT version the plugin is built against. Because the plugin performs a runtime API-version check, keep this in step with the NVGT version you ship alongside.

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
| `space_invaders.nvgt` | Audio game using `al_sound_pool` — 3D positioned loops, one-shots, HRTF |
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
