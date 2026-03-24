# VideoSurveillance

A C++/Qt desktop app that displays multiple RTSP streams (Tapo cameras) in a grid using a custom FFmpeg build. Built as a portfolio-grade, real-time video monitoring showcase.

## Screenshot

![VideoSurveillance live grid](docs/images/live-grid-overview.png)

## Features
- Dynamic camera grid built from `.env` entries.
- Reusable per-camera panels with live status badges.
- RTSP over TCP for stability.
- Automatic reconnect with retry backoff.
- Low-latency decode setup (FFmpeg `nobuffer`).
- `.env`-based configuration for camera endpoints.

## Tech Stack
- C++20
- Qt 6 (Widgets)
- FFmpeg (custom Windows build)
- CMake

## Project Structure
- `app/` App entry point.
- `core/` RTSP/FFmpeg streaming and `.env` loader.
- `ui/` Qt UI layer.
- `third_party/ffmpeg-msvc/` Local FFmpeg build (not versioned except examples).

## Requirements
- Windows
- Qt 6.10.1 (MSVC 2022 64-bit)
- FFmpeg build with `bin/`, `lib/`, `include/` placed at `third_party/ffmpeg-msvc/`
- CMake 3.20+

## Setup
1. Install Qt and set one of these environment variables:
   - `QT_ROOT` (recommended)
   - `QTDIR`

   On Windows, the project now also auto-detects `C:/Qt/*/msvc2022_64` if those variables are not set.

2. Place your FFmpeg build under `third_party/ffmpeg-msvc/` so these folders exist:
   - `third_party/ffmpeg-msvc/bin/`
   - `third_party/ffmpeg-msvc/lib/`
   - `third_party/ffmpeg-msvc/include/`

3. (Optional) Override the FFmpeg path via CMake:

```bash
cmake --preset x64-debug -DFFMPEG_ROOT=C:/path/to/ffmpeg
```

4. Create your `.env` file from the example:

```bash
copy .env.example .env
```

Then fill in your RTSP URLs in `.env`.

## Build
```bash
cmake --preset msvc-debug
cmake --build --preset msvc-debug-build
```

## Run
Launch the generated executable from `out/build/msvc-debug/Debug/` or press `F5` in VS Code.

## VS Code
- Use the `Run VideoSurveillance (msvc-debug)` launch configuration.
- `F5` runs configure and build automatically before launch.
- The older `x64-debug` preset still exists for developer-shell/Ninja workflows, but the VS Code path now defaults to the MSVC preset that does not require `cl.exe` in `PATH`.

## Configuration
`.env` variables:
- `VS_CAMERA_<n>_NAME`
- `VS_CAMERA_<n>_URL`
- `VS_CAMERA_<n>_ENABLED` (optional)

Example:
```
VS_CAMERA_1_NAME=Front Door
VS_CAMERA_1_URL=rtsp://user:pass@192.168.0.96:554/stream1
VS_CAMERA_2_NAME=Garage
VS_CAMERA_2_URL=rtsp://user:pass@192.168.0.159:554/stream1
VS_CAMERA_3_NAME=Backyard
VS_CAMERA_3_URL=rtsp://user:pass@192.168.0.222:554/stream1
```

Legacy variables `VS_RTSP_TC72`, `VS_RTSP_C210` and `VS_RTSP_TC70` are still accepted if no `VS_CAMERA_*` entries exist.

## Notes
- Do not commit `.env` files; they contain secrets.
- For best results on Tapo cameras, TCP transport is recommended.

## License
MIT
