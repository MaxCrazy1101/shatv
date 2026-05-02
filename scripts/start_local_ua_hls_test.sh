#!/usr/bin/env bash

set -euo pipefail

show_help() {
    cat <<'EOF'
用法:
  bash scripts/start_local_ua_hls_test.sh [input_video] [output_dir] [port] [user_agent]

说明:
  - 生成一个带音频的本地 VOD HLS 测试源
  - 启动会在控制台打印 HTTP User-Agent 的本地服务器
  - 写入临时 XDG_CONFIG_HOME 配置，方便 ShaTV 使用指定 User-Agent
  - 默认输入: docs/Big_Buck_Bunny_720_10s_1MB.mp4
  - 默认输出目录: /tmp/shatv-ua-hls
  - 默认端口: 18081
  - 默认 User-Agent: ShaTV-UA-Check/1.0

示例:
  bash scripts/start_local_ua_hls_test.sh
  bash scripts/start_local_ua_hls_test.sh ./local-media/sample.mp4 /tmp/shatv-ua-hls 18082 "ShaTV QA/1.0"

启动后按脚本输出的命令打开 ShaTV，服务器控制台应显示:
  UA path=/index.m3u8 user-agent=ShaTV-UA-Check/1.0
  UA path=/index0.ts user-agent=ShaTV-UA-Check/1.0

按 Ctrl-C 停止服务器。
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    show_help
    exit 0
fi

input_video=${1:-docs/Big_Buck_Bunny_720_10s_1MB.mp4}
output_dir=${2:-/tmp/shatv-ua-hls}
port=${3:-18081}
user_agent=${4:-ShaTV-UA-Check/1.0}
config_home=/tmp/shatv-ua-config
config_dir="${config_home}/shatv"
intermediate_mp4="${output_dir}/source-av.mp4"

if [[ ! -f "$input_video" ]]; then
    echo "输入视频不存在: $input_video" >&2
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

case "$output_dir" in
    /tmp/shatv-ua-hls|/tmp/shatv-ua-hls/*)
        ;;
    *)
        echo "为避免误删数据，输出目录必须位于 /tmp/shatv-ua-hls: $output_dir" >&2
        exit 1
        ;;
esac

mkdir -p "$output_dir" "$config_dir"
rm -f "$output_dir"/index.m3u8 "$output_dir"/index*.ts "$intermediate_mp4"

cat > "${config_dir}/config.toml" <<EOF
[network]
user_agent = "${user_agent}"

[epg]
url = ""

[ui]
[ui.osd]
auto_hide_seconds = 3

[playback]
volume = 50
muted = false
EOF

echo "生成带音频测试视频: $intermediate_mp4"
ffmpeg -hide_banner -loglevel error -y \
    -stream_loop 1 -i "$input_video" \
    -f lavfi -i sine=frequency=440:sample_rate=48000:duration=12 \
    -t 12 \
    -map 0:v:0 \
    -map 1:a:0 \
    -c:v libx264 \
    -preset veryfast \
    -g 60 \
    -keyint_min 60 \
    -sc_threshold 0 \
    -c:a aac \
    -shortest \
    "$intermediate_mp4"

echo "生成 HLS: ${output_dir}/index.m3u8"
ffmpeg -hide_banner -loglevel error -y \
    -i "$intermediate_mp4" \
    -c copy \
    -f hls \
    -hls_time 2 \
    -hls_list_size 0 \
    -hls_flags independent_segments \
    "${output_dir}/index.m3u8"

play_url="http://127.0.0.1:${port}/index.m3u8"

echo
echo "本地 UA/HLS 测试服务器即将启动"
echo "输出目录: $output_dir"
echo "播放地址: $play_url"
echo "临时配置: ${config_dir}/config.toml"
echo "期望 User-Agent: $user_agent"
echo
echo "另一个终端运行:"
echo "  env QT_QPA_PLATFORM=wayland XDG_CONFIG_HOME=${config_home} ./build/src/shatv --open-url \"${play_url}\""
echo
echo "服务器控制台会打印每个请求的 User-Agent。按 Ctrl-C 停止。"
echo

export SHATV_UA_HLS_DIR="$output_dir"
python3 - "$port" <<'PY'
import os
import sys
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


class UserAgentLoggingHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        print(
            f"UA path={self.path} user-agent={self.headers.get('User-Agent')}",
            flush=True,
        )
        super().do_GET()


port = int(sys.argv[1])
directory = os.environ["SHATV_UA_HLS_DIR"]
server = ThreadingHTTPServer(
    ("127.0.0.1", port),
    partial(UserAgentLoggingHandler, directory=directory),
)
server.serve_forever()
PY
