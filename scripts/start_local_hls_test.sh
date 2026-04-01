#!/usr/bin/env bash

set -euo pipefail

show_help() {
    cat <<'EOF'
用法:
  bash scripts/start_local_hls_test.sh <input_mp4> [output_dir] [port]

说明:
  - 使用本地媒体文件循环生成 HLS 测试流
  - 默认输出目录: /tmp/shatv-hls
  - 默认服务端口: 8080
  - 按 Ctrl-C 可同时停止 ffmpeg 和本地 HTTP 服务

示例:
  bash scripts/start_local_hls_test.sh /absolute/path/to/sample.mp4
  bash scripts/start_local_hls_test.sh ./local-media/sample.mp4 /tmp/shatv-hls 8080
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -lt 1 ]]; then
    show_help
    exit 0
fi

input_file=$1
output_dir=${2:-/tmp/shatv-hls}
port=${3:-8080}

if [[ ! -f "$input_file" ]]; then
    echo "输入文件不存在: $input_file" >&2
    exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "未找到 ffmpeg，请先安装 ffmpeg" >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "未找到 python3，请先安装 python3" >&2
    exit 1
fi

mkdir -p "$output_dir"
rm -f "$output_dir"/index.m3u8 "$output_dir"/index.m3u8.tmp "$output_dir"/index*.ts

ffmpeg_pid=""
http_pid=""

cleanup() {
    local exit_code=$?

    if [[ -n "$http_pid" ]] && kill -0 "$http_pid" >/dev/null 2>&1; then
        kill "$http_pid" >/dev/null 2>&1 || true
        wait "$http_pid" 2>/dev/null || true
    fi

    if [[ -n "$ffmpeg_pid" ]] && kill -0 "$ffmpeg_pid" >/dev/null 2>&1; then
        kill "$ffmpeg_pid" >/dev/null 2>&1 || true
        wait "$ffmpeg_pid" 2>/dev/null || true
    fi

    exit "$exit_code"
}

trap cleanup EXIT INT TERM

# 本地测试优先追求稳定可用，直接把媒体循环转成 HLS。
ffmpeg -re -stream_loop -1 \
    -i "$input_file" \
    -map 0:v:0 \
    -map 0:a:0? \
    -c:v libx264 \
    -preset veryfast \
    -c:a aac \
    -b:a 128k \
    -f hls \
    -hls_time 2 \
    -hls_list_size 6 \
    -hls_flags delete_segments+append_list+omit_endlist \
    "$output_dir/index.m3u8" &
ffmpeg_pid=$!

python3 -m http.server "$port" -d "$output_dir" >/dev/null 2>&1 &
http_pid=$!

echo "本地 HLS 已启动"
echo "输入文件: $input_file"
echo "输出目录: $output_dir"
echo "播放地址: http://127.0.0.1:${port}/index.m3u8"
echo "按 Ctrl-C 停止"

wait "$ffmpeg_pid"
