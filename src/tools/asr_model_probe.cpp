#include <sherpa-onnx/c-api/c-api.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

struct ProbeOptions {
    std::filesystem::path model_dir;
    std::filesystem::path audio_file;
    std::string encoder_name = "encoder.int8.onnx";
    std::string decoder_name = "decoder.int8.onnx";
    std::string tokens_name = "tokens.txt";
    std::string provider = "cpu";
    int32_t num_threads = 1;
};

struct WaveFixture {
    std::vector<float> samples;
    int32_t sample_rate = 0;
};

void PrintUsage(const char *program_name) {
    std::cerr
        << "Usage: " << program_name << " --model-dir <dir> --audio-file <wav> [options]\n"
        << "\n"
        << "Required files default to a streaming Paraformer int8 model layout:\n"
        << "  <dir>/encoder.int8.onnx\n"
        << "  <dir>/decoder.int8.onnx\n"
        << "  <dir>/tokens.txt\n"
        << "\n"
        << "Options:\n"
        << "  --encoder-name <file>  Override encoder file name\n"
        << "  --decoder-name <file>  Override decoder file name\n"
        << "  --tokens-name <file>   Override tokens file name\n"
        << "  --provider <name>      ONNX Runtime provider, default cpu\n"
        << "  --num-threads <n>      Inference thread count, default 1\n";
}

bool ReadValue(int argc, char **argv, int *index, std::string *value, std::string *error_message) {
    if (*index + 1 >= argc) {
        *error_message = std::string(argv[*index]) + " requires a value";
        return false;
    }
    *value = argv[++(*index)];
    return true;
}

bool IsSupportedProvider(const std::string &provider) {
    return provider == "cpu" || provider == "cuda" || provider == "coreml";
}

bool ParseOptions(int argc, char **argv, ProbeOptions *options, std::string *error_message) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        std::string value;
        if (argument == "--help" || argument == "-h") {
            return false;
        }
        if (argument == "--model-dir") {
            if (!ReadValue(argc, argv, &i, &value, error_message)) {
                return false;
            }
            options->model_dir = value;
        } else if (argument == "--audio-file") {
            if (!ReadValue(argc, argv, &i, &value, error_message)) {
                return false;
            }
            options->audio_file = value;
        } else if (argument == "--encoder-name") {
            if (!ReadValue(argc, argv, &i, &value, error_message)) {
                return false;
            }
            options->encoder_name = value;
        } else if (argument == "--decoder-name") {
            if (!ReadValue(argc, argv, &i, &value, error_message)) {
                return false;
            }
            options->decoder_name = value;
        } else if (argument == "--tokens-name") {
            if (!ReadValue(argc, argv, &i, &value, error_message)) {
                return false;
            }
            options->tokens_name = value;
        } else if (argument == "--provider") {
            if (!ReadValue(argc, argv, &i, &value, error_message)) {
                return false;
            }
            options->provider = value;
        } else if (argument == "--num-threads") {
            if (!ReadValue(argc, argv, &i, &value, error_message)) {
                return false;
            }
            char *end = nullptr;
            const long parsed_value = std::strtol(value.c_str(), &end, 10);
            if (end == value.c_str() || *end != '\0' ||
                parsed_value > std::numeric_limits<int32_t>::max() ||
                parsed_value < std::numeric_limits<int32_t>::min()) {
                *error_message = "--num-threads must be an integer";
                return false;
            }
            options->num_threads = static_cast<int32_t>(parsed_value);
        } else {
            *error_message = "unknown argument: " + argument;
            return false;
        }
    }

    if (options->model_dir.empty()) {
        *error_message = "missing required --model-dir";
        return false;
    }
    if (options->audio_file.empty()) {
        *error_message = "missing required --audio-file";
        return false;
    }
    if (options->num_threads <= 0) {
        *error_message = "--num-threads must be greater than zero";
        return false;
    }
    if (!IsSupportedProvider(options->provider)) {
        *error_message = "unsupported ASR provider: " + options->provider;
        return false;
    }
    return true;
}

bool RequireRegularFile(const std::filesystem::path &path, std::string *error_message) {
    std::error_code error_code;
    if (!std::filesystem::is_regular_file(path, error_code)) {
        *error_message = "required file is missing: " + path.string();
        return false;
    }
    return true;
}

uint16_t ReadU16Le(const std::array<char, 2> &bytes) {
    return static_cast<uint16_t>(static_cast<unsigned char>(bytes[0])) |
           static_cast<uint16_t>(static_cast<unsigned char>(bytes[1]) << 8);
}

uint32_t ReadU32Le(const std::array<char, 4> &bytes) {
    return static_cast<uint32_t>(static_cast<unsigned char>(bytes[0])) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[1])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[2])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[3])) << 24);
}

bool ReadExact(std::ifstream *stream, char *data, std::streamsize size) {
    stream->read(data, size);
    return stream->gcount() == size;
}

bool SkipBytes(std::ifstream *stream, uint32_t size) {
    stream->seekg(static_cast<std::streamoff>(size), std::ios::cur);
    return stream->good();
}

bool ReadPcm16Wave(const std::filesystem::path &path, WaveFixture *wave, std::string *error_message) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        *error_message = "failed to open WAV fixture: " + path.string();
        return false;
    }

    std::array<char, 4> riff_id{};
    std::array<char, 4> riff_size{};
    std::array<char, 4> wave_id{};
    if (!ReadExact(&stream, riff_id.data(), riff_id.size()) ||
        !ReadExact(&stream, riff_size.data(), riff_size.size()) ||
        !ReadExact(&stream, wave_id.data(), wave_id.size()) ||
        std::string(riff_id.data(), riff_id.size()) != "RIFF" ||
        std::string(wave_id.data(), wave_id.size()) != "WAVE") {
        *error_message = "fixture is not a RIFF/WAVE file: " + path.string();
        return false;
    }

    bool saw_format = false;
    bool saw_data = false;
    uint16_t audio_format = 0;
    uint16_t channel_count = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    std::vector<char> pcm_bytes;

    while (stream.good() && !saw_data) {
        std::array<char, 4> chunk_id{};
        std::array<char, 4> chunk_size_bytes{};
        if (!ReadExact(&stream, chunk_id.data(), chunk_id.size())) {
            break;
        }
        if (!ReadExact(&stream, chunk_size_bytes.data(), chunk_size_bytes.size())) {
            *error_message = "truncated WAV chunk header: " + path.string();
            return false;
        }

        const uint32_t chunk_size = ReadU32Le(chunk_size_bytes);
        const std::string chunk_name(chunk_id.data(), chunk_id.size());
        if (chunk_name == "fmt ") {
            if (chunk_size < 16) {
                *error_message = "WAV fmt chunk is too small: " + path.string();
                return false;
            }

            std::array<char, 2> u16{};
            std::array<char, 4> u32{};
            if (!ReadExact(&stream, u16.data(), u16.size())) {
                *error_message = "truncated WAV audio format: " + path.string();
                return false;
            }
            audio_format = ReadU16Le(u16);
            if (!ReadExact(&stream, u16.data(), u16.size())) {
                *error_message = "truncated WAV channel count: " + path.string();
                return false;
            }
            channel_count = ReadU16Le(u16);
            if (!ReadExact(&stream, u32.data(), u32.size())) {
                *error_message = "truncated WAV sample rate: " + path.string();
                return false;
            }
            sample_rate = ReadU32Le(u32);

            constexpr uint32_t kRemainingCoreFormatBytes = 6;
            if (!SkipBytes(&stream, kRemainingCoreFormatBytes)) {
                *error_message = "truncated WAV byte-rate/block-align fields: " + path.string();
                return false;
            }
            if (!ReadExact(&stream, u16.data(), u16.size())) {
                *error_message = "truncated WAV bits-per-sample field: " + path.string();
                return false;
            }
            bits_per_sample = ReadU16Le(u16);
            if (chunk_size > 16 && !SkipBytes(&stream, chunk_size - 16)) {
                *error_message = "truncated extended WAV fmt chunk: " + path.string();
                return false;
            }
            saw_format = true;
        } else if (chunk_name == "data") {
            pcm_bytes.resize(chunk_size);
            if (!ReadExact(&stream, pcm_bytes.data(), static_cast<std::streamsize>(pcm_bytes.size()))) {
                *error_message = "truncated WAV data chunk: " + path.string();
                return false;
            }
            saw_data = true;
        } else if (!SkipBytes(&stream, chunk_size)) {
            *error_message = "failed to skip WAV chunk: " + chunk_name;
            return false;
        }

        if (chunk_size % 2 == 1 && !SkipBytes(&stream, 1)) {
            *error_message = "failed to skip WAV padding byte: " + path.string();
            return false;
        }
    }

    if (!saw_format || !saw_data) {
        *error_message = "WAV fixture is missing fmt or data chunk: " + path.string();
        return false;
    }
    if (audio_format != 1 || bits_per_sample != 16 || channel_count == 0 || sample_rate == 0) {
        *error_message = "WAV fixture must be PCM signed 16-bit with at least one channel: " + path.string();
        return false;
    }

    const std::size_t bytes_per_frame = static_cast<std::size_t>(channel_count) * sizeof(int16_t);
    if (bytes_per_frame == 0 || pcm_bytes.size() % bytes_per_frame != 0) {
        *error_message = "WAV data size is not aligned to frame size: " + path.string();
        return false;
    }

    wave->sample_rate = static_cast<int32_t>(sample_rate);
    wave->samples.clear();
    wave->samples.reserve(pcm_bytes.size() / bytes_per_frame);

    for (std::size_t offset = 0; offset < pcm_bytes.size(); offset += bytes_per_frame) {
        int32_t mixed_sample = 0;
        for (uint16_t channel = 0; channel < channel_count; ++channel) {
            const std::size_t sample_offset = offset + static_cast<std::size_t>(channel) * sizeof(int16_t);
            const auto lo = static_cast<unsigned char>(pcm_bytes[sample_offset]);
            const auto hi = static_cast<unsigned char>(pcm_bytes[sample_offset + 1]);
            const int16_t sample = static_cast<int16_t>(static_cast<uint16_t>(lo) |
                                                        static_cast<uint16_t>(hi << 8));
            mixed_sample += sample;
        }
        const float mono_sample = static_cast<float>(mixed_sample) /
                                  static_cast<float>(channel_count) /
                                  32768.0F;
        wave->samples.push_back(mono_sample);
    }

    if (wave->samples.empty()) {
        *error_message = "WAV fixture contains no samples: " + path.string();
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char **argv) {
    ProbeOptions options;
    std::string error_message;
    if (!ParseOptions(argc, argv, &options, &error_message)) {
        if (!error_message.empty()) {
            std::cerr << "ShaTV ASR probe error: " << error_message << "\n\n";
        }
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::filesystem::path encoder_path = options.model_dir / options.encoder_name;
    const std::filesystem::path decoder_path = options.model_dir / options.decoder_name;
    const std::filesystem::path tokens_path = options.model_dir / options.tokens_name;
    const std::string encoder_path_text = encoder_path.string();
    const std::string decoder_path_text = decoder_path.string();
    const std::string tokens_path_text = tokens_path.string();

    // 先验证模型和音频夹具存在，避免 sherpa-onnx 初始化阶段给出含混的底层错误。
    if (!RequireRegularFile(encoder_path, &error_message) ||
        !RequireRegularFile(decoder_path, &error_message) ||
        !RequireRegularFile(tokens_path, &error_message) ||
        !RequireRegularFile(options.audio_file, &error_message)) {
        std::cerr << "ShaTV ASR probe error: " << error_message << '\n';
        return EXIT_FAILURE;
    }

    SherpaOnnxOnlineRecognizerConfig config{};
    config.feat_config.sample_rate = 16000;
    config.feat_config.feature_dim = 80;
    config.model_config.paraformer.encoder = encoder_path_text.c_str();
    config.model_config.paraformer.decoder = decoder_path_text.c_str();
    config.model_config.tokens = tokens_path_text.c_str();
    config.model_config.provider = options.provider.c_str();
    config.model_config.num_threads = options.num_threads;
    config.model_config.modeling_unit = "cjkchar";
    config.decoding_method = "greedy_search";
    config.max_active_paths = 4;
    config.enable_endpoint = 1;
    config.rule1_min_trailing_silence = 2.4F;
    config.rule2_min_trailing_silence = 1.2F;
    config.rule3_min_utterance_length = 20.0F;
    config.hotwords_score = 1.5F;

    // M3.1 探针只验证真实 recognizer 链路，不提供 mock 或静默成功路径。
    // shared SDK 的 C ABI 是更稳定的边界；C++ wrapper/示例 I/O 符号在部分包中
    // 可能和导出实现不一致。这里直接读取简单 PCM16 WAV fixture，并只调用 C API。
    WaveFixture wave;
    if (!ReadPcm16Wave(options.audio_file, &wave, &error_message)) {
        std::cerr << "ShaTV ASR probe error: " << error_message << '\n';
        return EXIT_FAILURE;
    }
    if (wave.samples.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        std::cerr << "ShaTV ASR probe error: WAV fixture is too large for one probe stream\n";
        return EXIT_FAILURE;
    }

    using OnlineRecognizerPtr = std::unique_ptr<
        const SherpaOnnxOnlineRecognizer,
        decltype(&SherpaOnnxDestroyOnlineRecognizer)>;
    OnlineRecognizerPtr recognizer(
        SherpaOnnxCreateOnlineRecognizer(&config),
        SherpaOnnxDestroyOnlineRecognizer);
    if (!recognizer) {
        std::cerr << "ShaTV ASR probe error: failed to create sherpa-onnx online recognizer\n";
        return EXIT_FAILURE;
    }

    using OnlineStreamPtr = std::unique_ptr<
        const SherpaOnnxOnlineStream,
        decltype(&SherpaOnnxDestroyOnlineStream)>;
    OnlineStreamPtr stream(
        SherpaOnnxCreateOnlineStream(recognizer.get()),
        SherpaOnnxDestroyOnlineStream);
    if (!stream) {
        std::cerr << "ShaTV ASR probe error: failed to create sherpa-onnx online stream\n";
        return EXIT_FAILURE;
    }

    SherpaOnnxOnlineStreamAcceptWaveform(
        stream.get(),
        wave.sample_rate,
        wave.samples.data(),
        static_cast<int32_t>(wave.samples.size()));
    SherpaOnnxOnlineStreamSetOption(stream.get(), "is_final", "1");
    SherpaOnnxOnlineStreamInputFinished(stream.get());

    while (SherpaOnnxIsOnlineStreamReady(recognizer.get(), stream.get()) != 0) {
        SherpaOnnxDecodeOnlineStream(recognizer.get(), stream.get());
    }

    using OnlineResultPtr = std::unique_ptr<
        const SherpaOnnxOnlineRecognizerResult,
        decltype(&SherpaOnnxDestroyOnlineRecognizerResult)>;
    OnlineResultPtr result(
        SherpaOnnxGetOnlineStreamResult(recognizer.get(), stream.get()),
        SherpaOnnxDestroyOnlineRecognizerResult);
    if (!result || result->text == nullptr || result->text[0] == '\0') {
        std::cerr << "ShaTV ASR probe error: recognizer returned empty text for fixture: "
                  << options.audio_file << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "ShaTV ASR probe ok text=" << result->text << '\n';
    return EXIT_SUCCESS;
}
