# ASR Model Acquisition Probe

## Scope

M3.1 keeps speech recognition out of the playback path. The normal ShaTV build does not require sherpa-onnx, ONNX Runtime, or model files. An ASR-enabled development build only adds the standalone `shatv_asr_probe` executable.

## Native Dependency Policy

- Configure ASR builds with `-DSHATV_ENABLE_ASR=ON`.
- Set `SHATV_SHERPA_ONNX_ROOT` to an extracted sherpa-onnx SDK that contains `include/sherpa-onnx/c-api/c-api.h` and the C API library.
- Set `SHATV_ONNXRUNTIME_ROOT` only when ONNX Runtime is supplied separately from the sherpa-onnx SDK.
- ShaTV does not require AUR packages as the development path.
- ShaTV does not use `FetchContent` to build sherpa-onnx or ONNX Runtime.
- ShaTV does not runtime-download native ASR libraries in M3.1. Runtime library deployment is a later plugin/bundle decision.

Example configure command:

```bash
cmake -B build-asr \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DSHATV_ENABLE_ASR=ON \
  -DSHATV_SHERPA_ONNX_ROOT=/opt/sherpa-onnx
```

If ONNX Runtime is supplied outside the sherpa-onnx SDK, add:

```bash
  -DSHATV_ONNXRUNTIME_ROOT=/opt/onnxruntime
```

## Manual Probe Flow

The first model candidate is the int8 bilingual Chinese/English streaming Paraformer package. The probe expects these files by default:

- `encoder.int8.onnx`
- `decoder.int8.onnx`
- `tokens.txt`

Candidate package notes:

- Source: `csukuangfj/sherpa-onnx-streaming-paraformer-bilingual-zh-en`.
- Upstream page: <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-paraformer/paraformer-models.html#csukuangfj-sherpa-onnx-streaming-paraformer-bilingual-zh-en-chinese-english>.
- Tested archive name: `sherpa-onnx-streaming-paraformer-bilingual-zh-en.tar.bz2`.
- Tested archive SHA-256: `5462a1fce42693deae572af1e8c4687124b12aa85fe61ff4d3168bb5280e205f`.
- Documented ONNX payload sizes: `encoder.int8.onnx` is about `158M`, and `decoder.int8.onnx` is about `68M`.
- The first probe uses CPU provider and 16 kHz feature extraction.
- The model package license and attribution requirements still need final review before bundling or offering an in-app download.

The probe calls sherpa-onnx through the C API. The C++ wrapper and example I/O
helpers are not part of ShaTV's ASR runtime boundary because some shared SDK
packages may not export every C++ helper symbol consistently.

Run the probe against a manually downloaded model directory and a local mono speech WAV fixture:

```bash
./build-asr/src/shatv_asr_probe \
  --model-dir /path/to/sherpa-onnx-streaming-paraformer-bilingual-zh-en \
  --audio-file /path/to/speech.wav
```

Override model file names when probing a non-int8 package:

```bash
./build-asr/src/shatv_asr_probe \
  --model-dir /path/to/model \
  --audio-file /path/to/speech.wav \
  --encoder-name encoder.onnx \
  --decoder-name decoder.onnx
```

The probe fails with an explicit error when required model files, the WAV fixture, recognizer creation, stream creation, or recognized text are missing. It does not provide a mock success path.

When both cache variables are set, CTest can run the real probe:

```bash
cmake -B build-asr \
  -DBUILD_TESTING=ON \
  -DSHATV_ENABLE_ASR=ON \
  -DSHATV_SHERPA_ONNX_ROOT=/opt/sherpa-onnx \
  -DSHATV_ASR_PROBE_MODEL_DIR=/path/to/model \
  -DSHATV_ASR_PROBE_AUDIO_FILE=/path/to/speech.wav

ctest --test-dir build-asr -R shatv_asr_probe --output-on-failure
```

## Playback Worker Probe

M3.3 adds a playback-side worker boundary, but it still does not expose subtitles
in QML. When `SHATV_ENABLE_ASR=ON`, playback starts one ASR worker session per
source attempt only if the runtime model directory is provided:

```bash
SHATV_ASR_MODEL_DIR=/path/to/sherpa-onnx-streaming-paraformer-bilingual-zh-en \
./build-asr/src/shatv --open-media /path/to/media-with-speech.mp4
```

Optional runtime overrides:

- `SHATV_ASR_ENCODER_NAME` defaults to `encoder.int8.onnx`
- `SHATV_ASR_DECODER_NAME` defaults to `decoder.int8.onnx`
- `SHATV_ASR_TOKENS_NAME` defaults to `tokens.txt`
- `SHATV_ASR_PROVIDER` defaults to `cpu`
- `SHATV_ASR_NUM_THREADS` defaults to `1`
- `SHATV_ASR_MAX_QUEUED_CHUNKS` defaults to `64`

The worker queue is bounded and non-blocking. Queue overflow returns an explicit
playback error instead of blocking FFmpeg decode or silently dropping stale ASR
input. Recognition results are logged during M3.3; subtitle bridge and overlay
state are M3.4 work.

## Future Online Model Service

Before ASR reaches the UI, model acquisition needs an explicit manifest and app-managed cache flow.

Initial manifest shape:

```json
{
  "id": "sherpa-onnx-streaming-paraformer-bilingual-zh-en-int8",
  "version": "upstream-release-or-model-revision",
  "display_name": "Streaming Paraformer bilingual zh/en int8",
  "source_url": "https://...",
  "archive_size_bytes": 0,
  "installed_size_bytes": 0,
  "sha256": "hex checksum",
  "license": "model license identifier",
  "attribution": "model source and required notice",
  "files": {
    "encoder": "encoder.int8.onnx",
    "decoder": "decoder.int8.onnx",
    "tokens": "tokens.txt"
  }
}
```

Service responsibilities:

- Detect whether the selected model is installed in the app data/cache directory.
- Show model size, source, version, checksum, license, and attribution before download.
- Download only model archives, not native ASR runtime libraries.
- Verify checksum before activation.
- Activate a model only when all required files exist.
- Support deletion and redownload from the UI.

## Windows Notes

The first Windows ASR probe should use a release x64 shared CPU sherpa-onnx package that matches the MSVC runtime strategy used by the app build. Runtime DLLs must be deployed next to `shatv_asr_probe.exe` or otherwise be discoverable by the Windows loader:

- sherpa-onnx C API DLL
- ONNX Runtime DLL
- any package-specific dependent DLLs

The base ShaTV Windows package remains free of ASR model files until the online model service and license flow are implemented.
