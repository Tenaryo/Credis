#!/usr/bin/env python3
"""
Credis vs Redis comprehensive benchmark driver.

Usage:
    python3 benchmarks/run_benchmarks.py [--quick] [--seed N]

Output: benchmarks/results/<timestamp>.md
"""

import subprocess
import sys
import json
import time
import os
import argparse
import random
import statistics
from datetime import datetime
from pathlib import Path
from typing import Optional

BENCH = os.environ.get("BENCH_REDIS_BENCHMARK", "/usr/bin/redis-benchmark")

# Servers — overridden by run_benchmarks.sh via env
REDIS_HOST = os.environ.get("BENCH_REDIS_HOST", "127.0.0.1")
REDIS_PORT = int(os.environ.get("BENCH_REDIS_PORT", "6379"))
CREDIS_HOST = os.environ.get("BENCH_CREDIS_HOST", "127.0.0.1")
CREDIS_PORT = int(os.environ.get("BENCH_CREDIS_PORT", "6380"))

REQUESTS = int(os.environ.get("BENCH_REQUESTS", "100000"))
PIPELINE_REQUESTS = int(os.environ.get("BENCH_PIPELINE_REQUESTS", "200000"))
QUICK_REQUESTS = int(os.environ.get("BENCH_QUICK_REQUESTS", "30000"))
QUICK_PIPELINE_REQUESTS = int(os.environ.get("BENCH_QUICK_PIPELINE_REQUESTS", "60000"))
REPEATS = 3
WARMUP_REQUESTS = 5000

CONCURRENCIES = [1, 10, 50, 100, 500]
PIPELINES = [1, 4, 8, 16, 32, 64]

# Commands: (name, needs_prepop_key, extra_args)
COMMANDS = [
    ("SET", None, None),
    ("GET", None, None),
    ("INCR", None, None),
    ("LPUSH", None, None),
    ("LPOP", "LPUSH", ["-r", "20000"]),
    ("ZADD", None, None),
    ("MSET", None, ["-r", "10"]),
    ("LRANGE", "LPUSH", None),
]


class BenchmarkRunner:
    def __init__(self, quick: bool = False):
        self.quick = quick
        self.n = QUICK_REQUESTS if quick else REQUESTS
        self.pipe_n = QUICK_PIPELINE_REQUESTS if quick else PIPELINE_REQUESTS
        self.results_dir = Path(__file__).parent / "results"
        self.results_dir.mkdir(parents=True, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.outfile = self.results_dir / f"{'quick_' if quick else ''}{ts}.md"
        self.out = []
        self._warmed_redis: set = set()
        self._warmed_credis: set = set()

    def write(self, line: str = "") -> None:
        self.out.append(line)
        if line:
            print(line)

    def flush(self) -> None:
        text = "\n".join(self.out)
        self.outfile.write_text(text + "\n")
        print(f"\nResults saved to {self.outfile}")

    def _warmup(self, host: str, port: int, cmd: str,
                concurrency: int = 10, pipeline: int = 1) -> None:
        args = [BENCH, "-h", host, "-p", str(port),
                "-n", str(WARMUP_REQUESTS), "-c", str(concurrency),
                "-P", str(pipeline), "-t", cmd, "-q"]
        subprocess.run(args, capture_output=True, timeout=30)

    def _warmup_once(self, host: str, port: int, cmd: str) -> None:
        """Warm up once per (server, command) — cache is per-server, not per-concurrency."""
        cache = self._warmed_redis if port == REDIS_PORT else self._warmed_credis
        if cmd not in cache:
            self._warmup(host, port, cmd)
            cache.add(cmd)

    def _run_bench(self, host: str, port: int, cmd: str, concurrency: int,
                   pipeline: int = 1, extra_args: Optional[list] = None,
                   latency: bool = False, requests: Optional[int] = None) -> dict:
        n = requests if requests is not None else self.n
        args = [BENCH, "-h", host, "-p", str(port),
                "-n", str(n), "-c", str(concurrency),
                "-P", str(pipeline), "-t", cmd]
        if extra_args:
            args.extend(extra_args)
        if not latency:
            args.append("-q")

        try:
            result = subprocess.run(args, capture_output=True, timeout=120)
            stdout_text = result.stdout.decode('utf-8', errors='replace')
            stderr_text = result.stderr.decode('utf-8', errors='replace')
        except subprocess.TimeoutExpired:
            return {"error": "timeout"}

        rps = 0.0
        for line in stdout_text.strip().split('\n'):
            line = line.strip()
            if 'throughput summary:' in line:
                parts = line.split()
                for i, p in enumerate(parts):
                    if p == 'requests' and i > 0:
                        try:
                            rps = float(parts[i - 1])
                        except ValueError:
                            pass
            elif 'requests per second' in line:
                parts = line.split()
                try:
                    rps = float(parts[0])
                except ValueError:
                    pass
        return {"rps": rps, "stdout": stdout_text.strip(), "stderr": stderr_text.strip()}

    def _run_bench_csv(self, host: str, port: int, cmd: str, concurrency: int,
                       pipeline: int = 1, extra_args: Optional[list] = None) -> float:
        args = [BENCH, "-h", host, "-p", str(port),
                "-n", str(self.n), "-c", str(concurrency),
                "-P", str(pipeline), "-t", cmd, "--csv"]
        if extra_args:
            args.extend(extra_args)
        try:
            result = subprocess.run(args, capture_output=True, timeout=120)
            stdout_text = result.stdout.decode('utf-8', errors='replace')
            for line in stdout_text.strip().split('\n'):
                line = line.strip()
                if not line or line.startswith('"test'):
                    continue
                parts = line.split(",")
                if len(parts) >= 2:
                    try:
                        return float(parts[1].strip('"'))
                    except ValueError:
                        pass
        except subprocess.TimeoutExpired:
            pass
        return 0.0

    def _bench_rps_repeated(self, host: str, port: int, cmd: str, concurrency: int,
                             pipeline: int = 1, extra_args: Optional[list] = None) -> float:
        """Warmup once per (server, command), then run REPEATS times, return median."""
        self._warmup_once(host, port, cmd)
        vals = []
        for _ in range(REPEATS):
            v = self._run_bench_csv(host, port, cmd, concurrency, pipeline, extra_args)
            if v > 0:
                vals.append(v)
        return statistics.median(vals) if vals else 0.0

    def _bench_latency_repeated(self, host: str, port: int, cmd: str,
                                 concurrency: int, pipeline: int) -> dict:
        """Run latency benchmark with warmup, repeats, return median p50/p95/p99/max."""
        self._warmup_once(host, port, cmd)
        self._warmup(host, port, cmd, concurrency, pipeline)
        all_p50, all_p95, all_p99, all_max, all_rps = [], [], [], [], []
        for _ in range(REPEATS):
            data = self._run_bench(host, port, cmd, concurrency, pipeline,
                                   latency=True, requests=100000)
            combined = (data.get("stdout", "") + "\n" + data.get("stderr", "")).replace('\r', '')
            p50 = p95 = p99 = pmax = 0.0
            lines = combined.split('\n')
            found = False
            for i, line in enumerate(lines):
                line = line.strip()
                if 'p50' in line and 'p95' in line and 'p99' in line:
                    if i + 1 < len(lines):
                        vals = lines[i + 1].split()
                        hdrs = line.split()
                        for j, hdr in enumerate(hdrs):
                            if j < len(vals):
                                try:
                                    v = float(vals[j])
                                    if hdr == 'p50': p50 = v
                                    elif hdr == 'p95': p95 = v
                                    elif hdr == 'p99': p99 = v
                                    elif hdr == 'max': pmax = v
                                except ValueError:
                                    pass
                    found = True
                    break
            if found:
                all_p50.append(p50)
                all_p95.append(p95)
                all_p99.append(p99)
                all_max.append(pmax)
                all_rps.append(data.get("rps", 0))
        if not all_p50:
            return {"p50": "-", "p95": "-", "p99": "-", "max": "-", "rps": 0}
        return {"p50": statistics.median(all_p50),
                "p95": statistics.median(all_p95),
                "p99": statistics.median(all_p99),
                "max": statistics.median(all_max),
                "rps": statistics.median(all_rps)}

    def _prepopulate(self, host: str, port: int, cmd: str) -> None:
        n = 20000 if not self.quick else 10000
        subprocess.run([BENCH, "-h", host, "-p", str(port),
                        "-n", str(n), "-c", "10", "-t", cmd, "-r", str(n)],
                       capture_output=True, timeout=30)

    def prepopulate_all(self) -> None:
        print("Pre-populating keys for ZREM, LPOP, LRANGE tests...")
        for host, port in [(REDIS_HOST, REDIS_PORT), (CREDIS_HOST, CREDIS_PORT)]:
            self._prepopulate(host, port, "ZADD")
            self._prepopulate(host, port, "LPUSH")

    # ----- Sections -----

    def section_info(self) -> None:
        redis_ver = os.environ.get("BENCH_REDIS_VERSION", "unknown")
        self.write("# Credis vs Redis Benchmark Report")
        self.write()
        self.write(f"**Date**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        self.write(f"**Redis**: {redis_ver} (port {REDIS_PORT})")
        self.write(f"**Credis**: port {CREDIS_PORT}")
        self.write(f"**Repeats**: {REPEATS} per data point (median)")
        self.write()

        governor = self._cpu_governor()
        if governor:
            self.write(f"**CPU governor**: {governor}")
            if governor != "performance":
                self.write()
                self.write("> :warning: CPU governor is **not** `performance`. "
                           "Results may have higher variance.")
            self.write()

    def section_throughput(self) -> None:
        """Non-pipeline throughput: all commands × all concurrencies."""
        self.write("## 1. Non-Pipeline Throughput")
        self.write()
        self.write(f"n={self.n} per benchmark")
        self.write()

        for cmd_name, _prepop_key, extra_args in COMMANDS:
            self.write(f"### {cmd_name}")
            self.write()
            header = "| Concurrency | Redis rps | Credis rps | Ratio | Winner |"
            sep =    "|------------:|----------:|-----------:|------:|--------|"
            self.write(header)
            self.write(sep)

            for c in CONCURRENCIES:
                servers = [(REDIS_HOST, REDIS_PORT, "Redis"),
                           (CREDIS_HOST, CREDIS_PORT, "Credis")]
                random.shuffle(servers)
                results_map = {}
                for host, port, label in servers:
                    results_map[label] = self._bench_rps_repeated(host, port, cmd_name, c, 1, extra_args)
                credis_rps = results_map["Credis"]
                redis_rps = results_map["Redis"]
                if redis_rps > 0:
                    ratio = credis_rps / redis_rps * 100
                    winner = "**Credis**" if ratio >= 100 else "Redis"
                else:
                    ratio = 0.0
                    winner = "N/A"
                self.write(f"| {c} | {redis_rps:>10,.1f} | {credis_rps:>10,.1f} | {ratio:>5.1f}% | {winner} |")
            self.write()
            sys.stdout.flush()

    def section_pipeline_throughput(self) -> None:
        """Pipeline throughput: SET/GET × all pipelines at c=50."""
        self.write("## 2. Pipeline Throughput (c=50)")
        self.write()
        self.write(f"n={self.pipe_n} per benchmark")
        self.write()

        for cmd in ("SET", "GET"):
            self.write(f"### {cmd}")
            self.write()
            header = "| P | Redis rps | Credis rps | Ratio | Winner |"
            sep =    "|---:|----------:|-----------:|------:|--------|"
            self.write(header)
            self.write(sep)

            for p in PIPELINES:
                servers = [(REDIS_HOST, REDIS_PORT, "Redis"),
                           (CREDIS_HOST, CREDIS_PORT, "Credis")]
                random.shuffle(servers)
                results_p = {}
                for host, port, label_p in servers:
                    results_p[label_p] = self._bench_rps_repeated(host, port, cmd, 50, p)
                credis_rps = results_p["Credis"]
                redis_rps = results_p["Redis"]
                if redis_rps > 0:
                    ratio = credis_rps / redis_rps * 100
                    winner = "**Credis**" if ratio >= 100 else "Redis"
                else:
                    ratio = 0.0
                    winner = "N/A"
                self.write(f"| {p} | {redis_rps:>10,.1f} | {credis_rps:>10,.1f} | {ratio:>5.1f}% | {winner} |")
            self.write()
            sys.stdout.flush()

    def section_latency(self) -> None:
        """Latency distribution for SET: c=1, c=50 at P=1 and P=64."""
        self.write("## 3. Latency Distribution (SET, n=100K)")
        self.write()

        for c in (1, 50):
            self.write(f"### c={c}")
            self.write()
            header = "| Server | P | p50 (ms) | p95 (ms) | p99 (ms) | max (ms) | rps |"
            sep    = "|--------|---|---------:|---------:|---------:|---------:|----:|"
            self.write(header)
            self.write(sep)

            for p in (1, 64):
                servers = [(REDIS_HOST, REDIS_PORT, "Redis"),
                           (CREDIS_HOST, CREDIS_PORT, "Credis")]
                random.shuffle(servers)
                results_lat = {}
                for host, port, label in servers:
                    results_lat[label] = self._bench_latency_repeated(host, port, "SET", c, p)
                for label in ("Redis", "Credis"):
                    v = results_lat[label]
                    def fmt(val, precision: int = 3) -> str:
                        if isinstance(val, str):
                            return val
                        return f"{val:.{precision}f}"
                    rps_str = f"{v['rps']:,.0f}" if isinstance(v['rps'], (int, float)) else str(v['rps'])
                    self.write(f"| {label} | {p} | {fmt(v['p50'])} | {fmt(v['p95'])} | "
                               f"{fmt(v['p99'])} | {fmt(v['max'])} | {rps_str} |")
            self.write()
            sys.stdout.flush()

    @staticmethod
    def _cpu_governor() -> str:
        try:
            path = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
            if os.path.isfile(path):
                with open(path) as f:
                    return f.read().strip()
        except OSError:
            pass
        return ""

    def run_all(self) -> None:
        self.section_info()
        self.section_throughput()
        self.section_pipeline_throughput()
        self.section_latency()
        self.flush()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Credis vs Redis benchmark suite")
    parser.add_argument("--quick", action="store_true", help="Quick mode (fewer requests)")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility")
    parser.add_argument("--section", choices=["throughput", "pipeline", "latency"],
                        help="Run only one section")
    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    runner = BenchmarkRunner(quick=args.quick)

    if args.section == "throughput":
        runner.section_throughput()
        runner.flush()
    elif args.section == "pipeline":
        runner.section_pipeline_throughput()
        runner.flush()
    elif args.section == "latency":
        runner.section_latency()
        runner.flush()
    else:
        runner.run_all()
