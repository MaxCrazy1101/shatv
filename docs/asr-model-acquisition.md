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
- `SHATV_ASR_BENCHMARK_LOG` defaults to `0`; set to `1` to log ASR benchmark metrics

The worker queue is bounded and non-blocking. Queue overflow returns an explicit
ASR error, stops the current ASR session, and clears subtitle state instead of
blocking FFmpeg decode or silently dropping stale ASR input.

## M3.5 Benchmark Run

Use the ASR-enabled application build and a real media source with speech. The
base app package still does not include model files.

```bash
env \
  SHATV_ASR_MODEL_DIR=/path/to/sherpa-onnx-streaming-paraformer-bilingual-zh-en \
  SHATV_ASR_BENCHMARK_LOG=1 \
  /usr/bin/time -v ./build-asr/src/shatv --open-media /path/to/media-with-speech.mp4 \
  2>&1 | tee build/asr-playback-benchmark.log
```

Collect at least these values from the run:

- process CPU and maximum RSS from `/usr/bin/time -v`;
- `ASR async startup completed elapsedMs=...`;
- `ASR benchmark chunk ... audioMs=... decodeMs=... rtf=... latencyMs=...`;
- `ASR benchmark summary ... averageRtf=... maxLatencyMs=... queueOverflows=...`;
- visible subtitle behavior during playback, especially whether lag is readable.

Interpretation for the first CPU/int8 Paraformer slice:

- `averageRtf < 1.0` means ASR can keep up with realtime audio on that machine.
- `queueOverflows=0` is required before treating a run as successful.
- Subtitle latency can be visibly delayed, but it must not stall playback or keep
  growing across endpoint resets.
- Benchmark `SHATV_ASR_NUM_THREADS=1` first. Test `2` only if `averageRtf` or
  visible latency is unacceptable.

Current M3.5 Linux/Wayland baseline from 2026-05-11:

| Metric | Value |
|--------|-------|
| Source | CCTV7 live playback |
| Model | `sherpa-onnx-streaming-paraformer-bilingual-zh-en` int8 |
| Provider | `cpu` |
| ASR startup | `808 ms` |
| Recognized audio in summary | `20607 ms` |
| ASR decode time in summary | `1755 ms` |
| Average RTF | `0.0851652` |
| Max ASR decode chunk | `57 ms` |
| Max ASR queue latency | `57 ms` |
| Queue overflows | `0` |
| Process CPU | `11%` |
| Process max RSS | `822332 KiB` |
| Wall time | `2:12.75` |

M3.5 decisions from this baseline:

- Keep the CPU int8 bilingual Paraformer as the first production model target.
- Keep `SHATV_ASR_NUM_THREADS=1` as the default. The measured RTF has enough
  headroom, and a higher default thread count would spend more CPU before there
  is evidence it improves subtitle readability.
- Use an initial visible subtitle latency budget of about 1 second. ASR queue
  latency should stay well below that; endpoint/finalization delay is accepted
  as the readability tradeoff for segment boundaries.
- Treat roughly 800 MiB RSS as the first measured ASR-enabled process-memory
  cost on Linux/Wayland. Re-check this on Windows before enabling ASR in a
  default portable package.

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
  "archive_sha256": "hex checksum",
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

- Detect whether the selected model is installed in the app-managed local data
  directory.
- Show model size, source, version, checksum, license, and attribution before download.
- Download only model archives, not native ASR runtime libraries.
- Verify checksum before activation.
- Activate a model only when all required files exist.
- Support deletion and redownload from the UI.

Storage locations:

- Installed/extracted models live under
  `QStandardPaths::AppLocalDataLocation/asr-models/<manifest.id>`. This keeps
  large persistent model data local to the machine, especially on Windows where
  `AppDataLocation` can resolve to roaming app data.
- Downloaded archives and partial downloads live under
  `QStandardPaths::CacheLocation/asr-model-archives`. These files are cache
  artifacts and may be removed/redownloaded without deleting an installed model.
- The exact native path depends on Qt's organization/application names. Typical
  mappings are local app data on Windows, `~/Library/Application Support` plus
  `~/Library/Caches` on macOS, and `~/.local/share` plus `~/.cache` on Linux.

M5.1 status service:

- `AsrModelService` exposes the compiled-in first manifest and checks the
  app-managed install directory.
- `SHATV_ASR_MODEL_DIR` remains a developer override. When it is set, ShaTV
  validates that directory first and does not silently fall back to an installed
  app-managed model.
- `SHATV_ASR_ENCODER_NAME`, `SHATV_ASR_DECODER_NAME`, and
  `SHATV_ASR_TOKENS_NAME` override the required file names for both developer
  and app-managed status checks.
- M5.1 only reports app-managed model status. Playback ASR startup still uses
  `SHATV_ASR_MODEL_DIR` until M5.4 connects managed model paths to the backend
  recognizer config.

M5.2a archive cache service:

- `AsrModelArchiveDownloader` uses `QNetworkAccessManager` to fetch the selected
  manifest's `source_url`.
- The archive is written to `<archive>.part` under
  `QStandardPaths::CacheLocation/asr-model-archives`.
- The `.part` file is renamed to the final cache path only after the downloaded
  bytes match `archive_sha256`.
- Existing cached archives are reused only after SHA256 verification.
- Download failure, cancellation, write failure, size mismatch, or checksum
  mismatch removes the `.part` file and does not replace an existing final
  archive.

Production model policy after M3.5:

- The base ShaTV package must not bundle model archives or extracted model files.
- The first user-facing production path is online model download on demand.
- A user-configured local model directory can remain available for development
  and advanced users, but it should not replace the app-managed download/cache
  flow.
- Model activation is per selected manifest version and requires checksum
  verification plus required-file checks before the subtitle toggle becomes
  available.

## Windows Notes

The first Windows ASR probe should use a release x64 shared CPU sherpa-onnx package that matches the MSVC runtime strategy used by the app build. Runtime DLLs must be deployed next to `shatv_asr_probe.exe` or otherwise be discoverable by the Windows loader:

- sherpa-onnx C API DLL
- ONNX Runtime DLL
- any package-specific dependent DLLs

The base ShaTV Windows package remains free of ASR model files until the online model service and license flow are implemented.

M3.5 Windows packaging decision:

- Keep Windows portable ASR as an optional ASR-capable package or add-on path,
  not the default base package.
- Ship native ASR runtime DLLs only in an ASR-enabled artifact after confirming
  the matching sherpa-onnx/ONNX Runtime release, dependency DLL list, and license
  notices.
- Do not ship model files inside the Windows artifact. Use the same app-managed
  online model download/cache policy as Linux.
