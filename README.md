# ShaTV

ShaTV is a cross-platform IPTV player built with C++20, Qt 6 Quick/QML, and an
FFmpeg playback core. The current application uses a pure QML shell, a C++
`AppShellBridge`, and `VideoPresenterItem` for video presentation.

Current capabilities:

- Open local media files, direct HTTP/HTTPS media URLs, local M3U playlists, and
  remote M3U playlists.
- Persist network settings such as `User-Agent` and EPG URL in `config.toml`.
- Play through the FFmpeg demux/decode/audio/video pipeline.
- Display channel list, recent items, programme information, and playback state
  in QML.
- Build optional ASR-enabled packages with sherpa-onnx, ONNX Runtime supplied by
  the sherpa-onnx SDK, and libarchive.
- Package Windows portable archives and Ubuntu `.deb` artifacts through
  `.github/workflows/ci.yml`.

## Source Layout

```text
.
├── CMakeLists.txt
├── docs
│   ├── architecture.md
│   ├── asr-model-acquisition.md
│   └── linux-packaging.md
├── packaging
├── scripts
├── src
│   ├── app
│   ├── application
│   ├── domain
│   ├── media
│   ├── player
│   ├── tools
│   └── ui
├── tests
│   └── unit
└── translations
```

## Dependencies

Linux development packages:

- Qt 6 Core, Network, Qml, Quick, QuickControls2, QuickDialogs2, Multimedia,
  ShaderTools, and LinguistTools.
- FFmpeg libraries: `libavformat`, `libavcodec`, `libavutil`, `libswresample`,
  and `libswscale`.
- `toml11`, or configure with `-DSHATV_FETCH_TOML11=ON`.
- `zlib`.
- Optional ASR install support: `libarchive`.

The base build does not require sherpa-onnx, ONNX Runtime, libarchive, or model
files. ASR support is enabled explicitly with `-DSHATV_ENABLE_ASR=ON`.

## Build And Test

Configure a normal development build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j2
timeout 60s ctest --test-dir build --output-on-failure
```

Run the application:

```bash
./build/src/shatv
```

Run FFmpeg smoke modes with local fixtures:

```bash
timeout 20s env QT_QPA_PLATFORM=offscreen \
  SHATV_FFMPEG_AUDIO_SMOKE_MEDIA=/absolute/path/to/audio.wav \
  ./build/src/shatv --ffmpeg-audio-smoke

timeout 20s env QT_QPA_PLATFORM=offscreen \
  SHATV_FFMPEG_SMOKE_MEDIA=/absolute/path/to/video.mp4 \
  ./build/src/shatv --ffmpeg-smoke
```

Open media from the command line:

```bash
./build/src/shatv --open-media ./local-media/sample.mp4
./build/src/shatv --open-media ./local-media/playlist.m3u
./build/src/shatv --open-media http://127.0.0.1:8080/live.m3u8
./build/src/shatv --open-url http://127.0.0.1:8080/index.m3u8
```

## Configuration

ShaTV stores user settings in the platform config location. On Linux this is
typically:

```text
~/.config/shatv/config.toml
```

Minimal example:

```toml
[network]
user_agent = "ShaTV Custom UA/1.0"
epg_url = "https://example.invalid/guide.xml"
```

The configured `User-Agent` is used for remote playlist fetches, EPG downloads,
and HTTP/HLS playback descriptors.

## ASR Build

ASR-enabled builds require an extracted sherpa-onnx SDK:

```bash
cmake -S . -B build-asr \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DSHATV_ENABLE_ASR=ON \
  -DSHATV_REQUIRE_LIBARCHIVE=ON \
  -DSHATV_SHERPA_ONNX_ROOT=/path/to/sherpa-onnx-sdk

cmake --build build-asr -j2
timeout 60s ctest --test-dir build-asr --output-on-failure
```

The selected sherpa-onnx SDK is expected to provide the C API header/library and
ONNX Runtime runtime/import libraries. Do not configure ASR by fetching or
building sherpa-onnx inside this repository.

Model acquisition and runtime details are tracked in
[docs/asr-model-acquisition.md](docs/asr-model-acquisition.md).

## Windows Packaging

Windows CI is defined in [.github/workflows/ci.yml](.github/workflows/ci.yml).
It builds:

- `shatv-windows-x64.zip`
- `shatv-windows-x64-asr.zip`

The workflow uses MSVC 2022, Qt 6.10.3 from `install-qt-action`, BtbN FFmpeg
shared builds, sherpa-onnx prebuilt runtime packages for ASR, and vcpkg
libarchive for the ASR package.

For local Windows packaging, always run `windeployqt` with the QML directory:

```powershell
windeployqt.exe --release --qmldir C:/path/to/shatv/src/ui/qml build-windows/src/shatv.exe
```

`--qmldir` is required so the portable package contains the QML modules used by
`MainWindow.qml`.

## Linux Packaging

Linux packaging details are in [docs/linux-packaging.md](docs/linux-packaging.md).
The CI workflow currently builds Ubuntu 26.04 `.deb` packages for the base and
ASR variants. AUR source package guidance is documented for `shatv-git`, and
binary release guidance is documented for `shatv-bin`.

## Local HLS Test

The repository does not store third-party test videos. Put local media outside
the repository or under ignored `local-media/`, then run:

```bash
bash scripts/start_local_hls_test.sh /absolute/path/to/input.mp4
```

The script starts an FFmpeg HLS loop and serves it at:

```text
http://127.0.0.1:8080/index.m3u8
```

## Documentation

- [Architecture](docs/architecture.md)
- [ASR model acquisition](docs/asr-model-acquisition.md)
- [Linux packaging](docs/linux-packaging.md)
