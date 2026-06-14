#!/usr/bin/env python3
"""
Credis vs Redis comprehensive benchmark driver.

Usage:
    python3 benchmarks/run_benchmarks.py [--quick] [--no-perf]

Output: benchmarks/results/<timestamp>.md
"""

import subprocess
import sys
import json
import time
import os
import argparse
from datetime import datetime
from pathlib import Path
from typing import Optional

BENCH = "/usr/bin/redis-benchmark"
PERF = "perf"

# Servers — overridden by run_benchmarks.sh via env
REDIS_HOST = os.environ.get("BENCH_REDIS_HOST", "127.0.0.1")
REDIS_PORT = int(os.environ.get("BENCH_REDIS_PORT", "6379"))
CREDIS_HOST = os.environ.get("BENCH_CREDIS_HOST", "127.0.0.1")
CREDIS_PORT = int(os.environ.get("BENCH_CREDIS_PORT", "6380"))

REQUESTS = int(os.environ.get("BENCH_REQUESTS", "100000"))
PIPELINE_REQUESTS = int(os.environ.get("BENCH_PIPELINE_REQUESTS", "200000"))
QUICK_REQUESTS = int(os.environ.get("BENCH_QUICK_REQUESTS", "30000"))
QUICK_PIPELINE_REQUESTS = int(os.environ.get("BENCH_QUICK_PIPELINE_REQUESTS", "60000"))

CONCURRENCIES = [1, 10, 50, 100, 500]
PIPELINES = [1, 4, 8, 16, 32, 64]

# Commands: (name, needs_prepop_key)
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
    def __init__(self, quick: bool = False, run_perf: bool = True):
        self.quick = quick
        self.run_perf = run_perf
        self.n = QUICK_REQUESTS if quick else REQUESTS
        self.pipe_n = QUICK_PIPELINE_REQUESTS if quick else PIPELINE_REQUESTS
        self.results_dir = Path(__file__).parent / "results"
        self.results_dir.mkdir(parents=True, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.outfile = self.results_dir / f"{'quick_' if quick else ''}{ts}.md"
        self.out = []

    def write(self, line: str = "") -> None:
        self.out.append(line)
        if line:
            print(line)

    def flush(self) -> None:
        text = "\n".join(self.out)
        self.outfile.write_text(text + "\n")
        print(f"\nResults saved to {self.outfile}")

    def _run_bench(self, host: str, port: int, cmd: str, concurrency: int,
                   pipeline: int = 1, extra_args: Optional[list] = None,
                   latency: bool = False, requests: Optional[int] = None) -> dict:
        n = requests if requests is not None else self.n
        args = [BENCH, "-h", host, "-p", str(port),
                "-n", str(n), "-c", str(concurrency),
                "-P", str(pipeline), "-t", cmd]
        if extra_args:
            args.extend(extra_args)
        if latency:
            args.extend(["--precision", "3"])
        else:
            args.append("-q")

        try:
            result = subprocess.run(args, capture_output=True, text=True, timeout=120)
        except subprocess.TimeoutExpired:
            return {"error": "timeout"}

        rps = 0.0
        for line in result.stdout.strip().split('\n'):
            line = line.strip()
            if 'throughput summary:' in line:
                # non-quiet format: "  throughput summary: 40766.26 requests per second"
                parts = line.split()
                for i, p in enumerate(parts):
                    if p == 'requests' and i > 0:
                        try:
                            rps = float(parts[i - 1])
                        except ValueError:
                            pass
            elif 'requests per second' in line:
                # quiet format: "SET: 40766.26 requests per second"
                parts = line.split()
                try:
                    rps = float(parts[0])
                except ValueError:
                    pass
        return {"rps": rps, "stdout": result.stdout.strip(), "stderr": result.stderr.strip()}

    def _run_bench_csv(self, host: str, port: int, cmd: str, concurrency: int,
                       pipeline: int = 1, extra_args: Optional[list] = None) -> float:
        args = [BENCH, "-h", host, "-p", str(port),
                "-n", str(self.n), "-c", str(concurrency),
                "-P", str(pipeline), "-t", cmd, "--csv"]
        if extra_args:
            args.extend(extra_args)
        try:
            result = subprocess.run(args, capture_output=True, text=True, timeout=120)
            for line in result.stdout.strip().split('\n'):
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
        self.write()

    def section_throughput(self) -> None:
        """Non-pipeline throughput: all commands × all concurrencies."""
        self.write("## 1. Non-Pipeline Throughput")
        self.write()
        self.write(f"n={self.n} per benchmark")
        self.write()

        for cmd_name, prepop_key, extra_args in COMMANDS:
            self.write(f"### {cmd_name}")
            self.write()
            header = "| Concurrency | Redis rps | Credis rps | Ratio | Winner |"
            sep =    "|------------:|----------:|-----------:|------:|--------|"
            self.write(header)
            self.write(sep)

            for c in CONCURRENCIES:
                redis_rps = self._run_bench_csv(REDIS_HOST, REDIS_PORT, cmd_name, c, 1, extra_args)
                credis_rps = self._run_bench_csv(CREDIS_HOST, CREDIS_PORT, cmd_name, c, 1, extra_args)
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
                redis_rps = self._run_bench_csv(REDIS_HOST, REDIS_PORT, cmd, 50, p)
                credis_rps = self._run_bench_csv(CREDIS_HOST, CREDIS_PORT, cmd, 50, p)
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

            for label, host, port in [("Redis", REDIS_HOST, REDIS_PORT),
                                       ("Credis", CREDIS_HOST, CREDIS_PORT)]:
                for p in (1, 64):
                    data = self._run_bench(host, port, "SET", c, p, latency=True, requests=100000)
                    stdout = data.get("stdout", "")
                    rps_str = data.get("rps", 0)

                    p50 = p95 = p99 = pmax = "-"
                    # Parse table-format latency output:
                    #           avg       min       p50       p95       p99       max
                    #         0.025     0.015     0.023     0.031     0.047     0.271
                    lines = stdout.split('\n')
                    for i, line in enumerate(lines):
                        if 'p50' in line and 'p95' in line and 'p99' in line:
                            if i + 1 < len(lines):
                                vals = lines[i + 1].split()
                                hdrs = line.split()
                                for j, hdr in enumerate(hdrs):
                                    if j < len(vals):
                                        try:
                                            v = f"{float(vals[j]):.3f}"
                                            if hdr == 'p50': p50 = v
                                            elif hdr == 'p95': p95 = v
                                            elif hdr == 'p99': p99 = v
                                            elif hdr == 'max': pmax = v
                                        except (ValueError, IndexError):
                                            pass
                            break

                    self.write(f"| {label} | {p} | {p50} | {p95} | {p99} | {pmax} | {rps_str:,.0f} |")
            self.write()
            sys.stdout.flush()

    def section_perf_stat(self) -> None:
        """perf stat for key scenarios."""
        if not self.run_perf:
            return

        self.write("## 4. Perf Stat (SET+GET, c=50)")
        self.write()

        for label, host, port in [("Credis", CREDIS_HOST, CREDIS_PORT),
                                   ("Redis", REDIS_HOST, REDIS_PORT)]:
            self.write(f"### {label}")
            self.write()

            for p in (1, 8, 64):
                self.write(f"**P={p}**")
                self.write("```")
                n = self.n if p == 1 else self.pipe_n
                result = subprocess.run(
                    [PERF, "stat", "-e",
                     "task-clock,cycles,instructions,branches,branch-misses,L1-dcache-loads,L1-dcache-load-misses",
                     BENCH, "-h", host, "-p", str(port),
                     "-n", str(n), "-c", "50", "-P", str(p), "-t", "set,get", "-q"],
                    capture_output=True, text=True, timeout=120
                )
                for line in result.stderr.split('\n'):
                    line = line.strip()
                    if line and 'WARNING' not in line and 'Performance counter' not in line:
                        self.write(line)
                self.write("```")
                self.write()
            sys.stdout.flush()

    def run_all(self) -> None:
        self.section_info()
        self.section_throughput()
        self.section_pipeline_throughput()
        self.section_latency()
        self.section_perf_stat()
        self.flush()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Credis vs Redis benchmark suite")
    parser.add_argument("--quick", action="store_true", help="Quick mode (fewer requests)")
    parser.add_argument("--no-perf", action="store_true", help="Skip perf stat")
    parser.add_argument("--section", choices=["throughput", "pipeline", "latency", "perf"],
                        help="Run only one section")
    args = parser.parse_args()

    runner = BenchmarkRunner(quick=args.quick, run_perf=not args.no_perf)

    if args.section == "throughput":
        runner.section_throughput()
        runner.flush()
    elif args.section == "pipeline":
        runner.section_pipeline_throughput()
        runner.flush()
    elif args.section == "latency":
        runner.section_latency()
        runner.flush()
    elif args.section == "perf":
        runner.section_perf_stat()
        runner.flush()
    else:
        runner.run_all()
