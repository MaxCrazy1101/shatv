#include <sherpa-onnx/c-api/cxx-api.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
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

    // 先验证模型和音频夹具存在，避免 sherpa-onnx 初始化阶段给出含混的底层错误。
    if (!RequireRegularFile(encoder_path, &error_message) ||
        !RequireRegularFile(decoder_path, &error_message) ||
        !RequireRegularFile(tokens_path, &error_message) ||
        !RequireRegularFile(options.audio_file, &error_message)) {
        std::cerr << "ShaTV ASR probe error: " << error_message << '\n';
        return EXIT_FAILURE;
    }

    namespace sherpa = sherpa_onnx::cxx;

    sherpa::OnlineRecognizerConfig config;
    config.feat_config.sample_rate = 16000;
    config.feat_config.feature_dim = 80;
    config.model_config.paraformer.encoder = encoder_path.string();
    config.model_config.paraformer.decoder = decoder_path.string();
    config.model_config.tokens = tokens_path.string();
    config.model_config.provider = options.provider;
    config.model_config.num_threads = options.num_threads;
    config.decoding_method = "greedy_search";
    config.enable_endpoint = true;

    // M3.1 探针只验证真实 recognizer 链路，不提供 mock 或静默成功路径。
    const auto wave = sherpa::ReadWave(options.audio_file.string());
    if (wave.samples.empty() || wave.sample_rate <= 0) {
        std::cerr << "ShaTV ASR probe error: failed to read mono WAV fixture: "
                  << options.audio_file << '\n';
        return EXIT_FAILURE;
    }
    if (wave.samples.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        std::cerr << "ShaTV ASR probe error: WAV fixture is too large for one probe stream\n";
        return EXIT_FAILURE;
    }

    auto recognizer = sherpa::OnlineRecognizer::Create(config);
    if (!recognizer.Get()) {
        std::cerr << "ShaTV ASR probe error: failed to create sherpa-onnx online recognizer\n";
        return EXIT_FAILURE;
    }

    auto stream = recognizer.CreateStream();
    if (!stream.Get()) {
        std::cerr << "ShaTV ASR probe error: failed to create sherpa-onnx online stream\n";
        return EXIT_FAILURE;
    }

    stream.AcceptWaveform(
        wave.sample_rate,
        wave.samples.data(),
        static_cast<int32_t>(wave.samples.size()));
    stream.InputFinished();

    while (recognizer.IsReady(&stream)) {
        recognizer.Decode(&stream);
    }

    const sherpa::OnlineRecognizerResult result = recognizer.GetResult(&stream);
    if (result.text.empty()) {
        std::cerr << "ShaTV ASR probe error: recognizer returned empty text for fixture: "
                  << options.audio_file << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "ShaTV ASR probe ok text=" << result.text << '\n';
    return EXIT_SUCCESS;
}
