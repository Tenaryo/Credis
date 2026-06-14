#!/usr/bin/env bash
set -euo pipefail
#
# run_benchmarks.sh — 一键运行 Credis vs Redis Benchmark
#
# Usage:
#   ./run_benchmarks.sh            # Full benchmark
#   ./run_benchmarks.sh --quick     # Quick mode (fewer requests, faster)
#   ./run_benchmarks.sh --help
#
# Self-contained: auto-builds Credis (Release) and a pinned Redis version.
# Works on Linux, macOS, and WSL — no pre-installed Redis required.
#
# Output: benchmarks/results/<timestamp>.md

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REDIS_VERSION="7.2.8"
REDIS_PREFIX="$SCRIPT_DIR/build/_redis"
REDIS_SERVER="$REDIS_PREFIX/bin/redis-server"
REDIS_BENCH="$REDIS_PREFIX/bin/redis-benchmark"
REDIS_CLI="$REDIS_PREFIX/bin/redis-cli"
CREDIS_BIN="$SCRIPT_DIR/build/redis"

REDIS_PORT=6381
CREDIS_PORT=6382

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

QUICK=false
EXTRA_ARGS=""

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Options:
  --quick        Quick mode (30K reqs per test, faster)
  --no-perf      Skip perf stat section
  --section S    Run only one section: throughput|pipeline|latency|perf
  --help         Show this help

Output: benchmarks/results/<timestamp>.md
EOF
    exit 0
}

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --quick)    QUICK=true; EXTRA_ARGS="--quick" ;;
        --no-perf)  EXTRA_ARGS="$EXTRA_ARGS --no-perf" ;;
        --section)  EXTRA_ARGS="$EXTRA_ARGS --section $2"; shift ;;
        --help)     usage ;;
        *)          echo "Unknown option: $1"; usage ;;
    esac
    shift
done

# --------------------------------------------------
# Platform detection
# --------------------------------------------------
detect_nproc() {
    if command -v nproc &>/dev/null; then
        nproc
    elif [[ "$(uname -s)" == "Darwin" ]] || [[ "$(uname -s)" == "Linux" ]]; then
        getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4
    else
        echo 4
    fi
}

detect_os() {
    case "$(uname -s)" in
        Linux*)  echo "linux" ;;
        Darwin*) echo "macos" ;;
        *)       echo "unknown" ;;
    esac
}

# --------------------------------------------------
# Setup Redis (auto-build if not present)
# --------------------------------------------------
setup_redis() {
    if [[ -x "$REDIS_SERVER" ]] && [[ -x "$REDIS_BENCH" ]]; then
        local installed_ver
        installed_ver=$("$REDIS_SERVER" --version 2>&1 | head -1)
        echo -e "${GREEN}Redis found: $installed_ver${NC}"
        return 0
    fi

    echo -e "${YELLOW}Redis $REDIS_VERSION not found. Building from source...${NC}"

    local build_dir="$SCRIPT_DIR/build/_redis_build"
    rm -rf "$build_dir"
    mkdir -p "$build_dir"

    local src="redis-${REDIS_VERSION}.tar.gz"
    local url="https://download.redis.io/releases/${src}"

    echo -e "${YELLOW}Downloading $url ...${NC}"
    if command -v curl &>/dev/null; then
        curl -sSL "$url" -o "$build_dir/$src"
    elif command -v wget &>/dev/null; then
        wget -q "$url" -O "$build_dir/$src"
    else
        echo -e "${RED}Error: neither curl nor wget found. Install one of them.${NC}"
        exit 1
    fi

    echo -e "${YELLOW}Extracting...${NC}"
    tar xzf "$build_dir/$src" -C "$build_dir"
    rm "$build_dir/$src"

    local src_dir="$build_dir/redis-${REDIS_VERSION}"

    local nproc
    nproc=$(detect_nproc)

    echo -e "${YELLOW}Building Redis $REDIS_VERSION (jobs=$nproc)...${NC}"
    make -C "$src_dir" -j"$nproc" > /dev/null 2>&1

    mkdir -p "$REDIS_PREFIX/bin"
    cp "$src_dir/src/redis-server" "$REDIS_PREFIX/bin/"
    cp "$src_dir/src/redis-benchmark" "$REDIS_PREFIX/bin/"
    cp "$src_dir/src/redis-cli" "$REDIS_PREFIX/bin/"

    rm -rf "$build_dir"

    echo -e "${GREEN}Redis $REDIS_VERSION built successfully${NC}"
}

# --------------------------------------------------
# Setup Credis (auto-build if not present)
# --------------------------------------------------
setup_credis() {
    if [[ -x "$CREDIS_BIN" ]]; then
        return 0
    fi
    echo -e "${YELLOW}Credis binary not found. Building Release...${NC}"
    "$SCRIPT_DIR/build.sh" Release
}

# --------------------------------------------------
# Check if perf is available (Linux only)
# --------------------------------------------------
check_perf() {
    if [[ "$(detect_os)" != "linux" ]]; then
        echo -e "${YELLOW}perf stat is Linux-only; skipping perf section${NC}"
        EXTRA_ARGS="$EXTRA_ARGS --no-perf"
    elif ! command -v perf &>/dev/null; then
        echo -e "${YELLOW}perf not found; skipping perf section${NC}"
        EXTRA_ARGS="$EXTRA_ARGS --no-perf"
    fi
}

# --------------------------------------------------
# Port helpers (cross-platform)
# --------------------------------------------------
check_port_free() {
    local port=$1
    if [[ "$(detect_os)" == "macos" ]]; then
        ! lsof -i "tcp:$port" -sTCP:LISTEN &>/dev/null
    else
        ! ss -tlnp 2>/dev/null | grep -q ":$port "
    fi
}

wait_port_free() {
    local port=$1
    local max_wait=${2:-3}
    for i in $(seq 1 $max_wait); do
        if check_port_free "$port"; then
            return 0
        fi
        sleep 1
    done
    return 1
}

force_kill_port() {
    local port=$1
    if [[ "$(detect_os)" == "macos" ]]; then
        local pid
        pid=$(lsof -ti "tcp:$port" -sTCP:LISTEN 2>/dev/null || true)
        [[ -n "$pid" ]] && kill -9 "$pid" 2>/dev/null || true
    else
        fuser -k "$port/tcp" 2>/dev/null || true
        sleep 0.3
    fi
    pkill -9 -f "redis-server.*:$port" 2>/dev/null || true
    pkill -9 -f "build/redis.*:$port" 2>/dev/null || true
}

# --------------------------------------------------
# Main
# --------------------------------------------------
echo -e "${BOLD}${BLUE}========================================${NC}"
echo -e "${BOLD}${BLUE}  Credis vs Redis Benchmark Suite${NC}"
echo -e "${BOLD}${BLUE}========================================${NC}"
echo

setup_redis
setup_credis
check_perf

# PIDs for cleanup
REDIS_PID=""
CREDIS_PID=""

cleanup() {
    echo -e "${BLUE}Shutting down servers...${NC}"
    [[ -n "$REDIS_PID" ]] && kill -TERM "$REDIS_PID" 2>/dev/null || true
    [[ -n "$CREDIS_PID" ]] && kill -TERM "$CREDIS_PID" 2>/dev/null || true
    sleep 0.3
    [[ -n "$REDIS_PID" ]] && kill -KILL "$REDIS_PID" 2>/dev/null || true
    [[ -n "$CREDIS_PID" ]] && kill -KILL "$CREDIS_PID" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT

# Kill any lingering servers
echo -e "${YELLOW}Cleaning up ports...${NC}"
force_kill_port $REDIS_PORT
force_kill_port $CREDIS_PORT

if ! wait_port_free $REDIS_PORT 5; then
    echo -e "${RED}Port $REDIS_PORT still in use. Aborting.${NC}"
    exit 1
fi
if ! wait_port_free $CREDIS_PORT 5; then
    echo -e "${RED}Port $CREDIS_PORT still in use. Aborting.${NC}"
    exit 1
fi
echo -e "${GREEN}Ports $REDIS_PORT and $CREDIS_PORT are free${NC}"

# Start Redis
echo -e "${YELLOW}Starting Redis $REDIS_VERSION on port $REDIS_PORT...${NC}"
"$REDIS_SERVER" --port $REDIS_PORT --save "" --appendonly no --loglevel warning &
REDIS_PID=$!
sleep 2
if ! kill -0 "$REDIS_PID" 2>/dev/null; then
    echo -e "${RED}Redis failed to start (PID $REDIS_PID died)${NC}"
    exit 1
fi
if ! "$REDIS_CLI" -p $REDIS_PORT PING &>/dev/null; then
    echo -e "${RED}Redis failed to respond to PING${NC}"
    exit 1
fi
echo -e "${GREEN}Redis OK (PID $REDIS_PID)${NC}"

# Start Credis
echo -e "${YELLOW}Starting Credis on port $CREDIS_PORT...${NC}"
"$CREDIS_BIN" --port $CREDIS_PORT &
CREDIS_PID=$!
sleep 1
if ! kill -0 "$CREDIS_PID" 2>/dev/null; then
    echo -e "${RED}Credis failed to start (PID $CREDIS_PID died)${NC}"
    exit 1
fi
if ! "$REDIS_CLI" -p $CREDIS_PORT PING &>/dev/null; then
    echo -e "${RED}Credis failed to respond to PING${NC}"
    exit 1
fi
echo -e "${GREEN}Credis OK (PID $CREDIS_PID)${NC}"
echo

# Pre-populate both servers
echo -e "${YELLOW}Pre-populating keys...${NC}"
python3 -c "
import subprocess
B='$REDIS_BENCH'
for p in ($REDIS_PORT, $CREDIS_PORT):
    for cmd, n, rng in [('ZADD', 20000, 20000), ('LPUSH', 20000, 20000), ('RPUSH', 20000, 20000)]:
        subprocess.run([B, '-h', '127.0.0.1', '-p', str(p), '-n', str(n), '-c', '10', '-t', cmd, '-r', str(rng)],
                       capture_output=True, timeout=20)
print('Done')
"
echo

# Run benchmark
echo -e "${YELLOW}Running benchmarks...${NC}"
echo
export BENCH_REDIS_PORT=$REDIS_PORT
export BENCH_CREDIS_PORT=$CREDIS_PORT

# Pass Redis version info to the Python reporter
export BENCH_REDIS_VERSION=$("$REDIS_SERVER" --version 2>&1 | head -1)

python3 "$SCRIPT_DIR/benchmarks/run_benchmarks.py" $EXTRA_ARGS

echo
echo -e "${GREEN}${BOLD}Benchmark complete!${NC}"
