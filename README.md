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

    // Load raw PCM into a buffer (see examples/wav_loader.nvgt for a WAV helper)
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
| `3d_positioning.nvgt` | Move a sound source in 3D space |
| `efx_reverb.nvgt` | Reverb via EFX auxiliary effect slot |
| `efx_echo.nvgt` | Echo effect with adjustable delay and feedback |
| `efx_lowpass.nvgt` | Lowpass direct filter (muffling effect) |
| `streaming.nvgt` | Buffer-queue streaming for large files |
| `capture.nvgt` | Microphone capture and playback |
| `hrtf.nvgt` | HRTF model selection and 3D audio |
| `device_info.nvgt` | List all devices, extensions, and capabilities |

All examples that play audio expect `test.wav` (PCM, any bit depth, mono recommended for 3D) in the `examples/` folder.

## License

This software is provided as-is. Do whatever you want with it.
