#!/usr/bin/env bash
set -euo pipefail
#
# run_benchmarks.sh — 一键运行 Credis vs Redis 完整 Benchmark
#
# Usage:
#   ./run_benchmarks.sh            # Full benchmark
#   ./run_benchmarks.sh --quick     # Quick mode (fewer requests, faster)
#   ./run_benchmarks.sh --help
#
# Prerequisites:
#   - redis-server and redis-benchmark installed
#   - Credis built (./build.sh Release)
#   - perf available for perf stat section

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CREDIS_BIN="$SCRIPT_DIR/build/redis"
REDIS_SERVER="$(command -v redis-server 2>/dev/null || echo /usr/bin/redis-server)"
REDIS_BENCH="$(command -v redis-benchmark 2>/dev/null || echo /usr/bin/redis-benchmark)"

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

# Check dependencies
for cmd in "$REDIS_SERVER" "$REDIS_BENCH"; do
    if ! command -v "$cmd" &>/dev/null; then
        echo -e "${RED}Error: $cmd not found${NC}"
        exit 1
    fi
done

if [ ! -x "$CREDIS_BIN" ]; then
    echo -e "${YELLOW}Credis binary not found. Building...${NC}"
    "$SCRIPT_DIR/build.sh" Release
fi

# PIDs for cleanup
REDIS_PID=""
CREDIS_PID=""

cleanup() {
    echo -e "${BLUE}Shutting down servers...${NC}"
    [ -n "$REDIS_PID" ] && kill -TERM "$REDIS_PID" 2>/dev/null || true
    [ -n "$CREDIS_PID" ] && kill -TERM "$CREDIS_PID" 2>/dev/null || true
    sleep 0.3
    [ -n "$REDIS_PID" ] && kill -KILL "$REDIS_PID" 2>/dev/null || true
    [ -n "$CREDIS_PID" ] && kill -KILL "$CREDIS_PID" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT

force_kill_port() {
    local port=$1
    # Kill any process explicitly bound to the port
    fuser -k "$port/tcp" 2>/dev/null || true
    sleep 0.3
    # Fallback: pkill by name
    pkill -9 -f "redis-server.*:$port" 2>/dev/null || true
    pkill -9 -f "build/redis.*:$port" 2>/dev/null || true
}

wait_port_free() {
    local port=$1
    local max_wait=${2:-3}
    for i in $(seq 1 $max_wait); do
        if ! ss -tlnp 2>/dev/null | grep -q ":$port "; then
            return 0
        fi
        sleep 1
    done
    return 1
}

echo -e "${BOLD}${BLUE}========================================${NC}"
echo -e "${BOLD}${BLUE}  Credis vs Redis Benchmark Suite${NC}"
echo -e "${BOLD}${BLUE}========================================${NC}"
echo

# Kill any lingering servers and wait for ports to free
echo -e "${YELLOW}Cleaning up ports...${NC}"
force_kill_port $REDIS_PORT
force_kill_port $CREDIS_PORT

if ! wait_port_free $REDIS_PORT 5; then
    echo -e "${RED}Port $REDIS_PORT still in use after cleanup. Aborting.${NC}"
    exit 1
fi
if ! wait_port_free $CREDIS_PORT 5; then
    echo -e "${RED}Port $CREDIS_PORT still in use after cleanup. Aborting.${NC}"
    exit 1
fi
echo -e "${GREEN}Ports $REDIS_PORT and $CREDIS_PORT are free${NC}"

# Start Redis
echo -e "${YELLOW}Starting Redis on port $REDIS_PORT...${NC}"
"$REDIS_SERVER" --port $REDIS_PORT --save "" --appendonly no --loglevel warning &
REDIS_PID=$!
sleep 2
if ! kill -0 "$REDIS_PID" 2>/dev/null; then
    echo -e "${RED}Redis failed to start (PID $REDIS_PID died)${NC}"
    exit 1
fi
if ! redis-cli -p $REDIS_PORT PING &>/dev/null; then
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
if ! redis-cli -p $CREDIS_PORT PING &>/dev/null; then
    echo -e "${RED}Credis failed to respond to PING${NC}"
    exit 1
fi
echo -e "${GREEN}Credis OK (PID $CREDIS_PID)${NC}"
echo

# Pre-populate both servers
echo -e "${YELLOW}Pre-populating keys...${NC}"
python3 -c "
import subprocess, sys
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

python3 "$SCRIPT_DIR/benchmarks/run_benchmarks.py" $EXTRA_ARGS

echo
echo -e "${GREEN}${BOLD}Benchmark complete!${NC}"
