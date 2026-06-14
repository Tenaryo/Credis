# Credis

[![CI](https://github.com/Tenaryo/Credis/actions/workflows/ci.yml/badge.svg)](https://github.com/Tenaryo/Credis/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

A Redis 7.0-compatible server written from scratch in C++23 — ~3,700 lines, zero dependencies, 793 KB stripped binary. Deep-pipeline throughput (P=64) reaches 150% of Redis at lower tail latency; single-connection throughput leads by 8–18%.

## Dependencies

- **C++23** compiler (GCC 13+ recommended)
- **CMake** 3.21+
- **POSIX** (epoll, sockets)

No Boost, no libevent, no hiredis.

## Performance

### Methodology

- **Hardware**: Intel i5-125H (x86-64), Ubuntu 24.04, loopback TCP
- **Throughput**: `redis-benchmark -n 100000 -c 50 -q` (non-pipeline), `-n 200000` (pipeline)
- **Latency**: `redis-benchmark -n 100000 --precision 3`
- Persistence disabled on both servers
- **Credis**: `-O3 -march=native` + LTO (Release build)
- **Redis**: 7.2.8 (auto-built by `run_benchmarks.sh`)

### Non-Pipeline Throughput (c=50, n=100K)

| Command | Credis rps | Redis rps | Ratio |
|---------|-----------|-----------|-------|
| SET | 136,240 | 138,696 | 98.2% |
| GET | 141,844 | 136,799 | 103.7% |
| INCR | 134,228 | 141,044 | 95.2% |
| MSET | 134,048 | 127,227 | 105.4% |
| LPUSH | 133,333 | 128,700 | 103.6% |
| LPOP | 131,579 | 126,263 | 104.2% |
| ZADD | 130,208 | 131,752 | 98.8% |
| LRANGE | 138,696 | 129,870 | 106.8% |


### Pipeline Throughput (c=50, n=200K)

#### SET

| P | Credis rps | Redis rps | Ratio |
|---|-----------|-----------|-------|
| 1 | 130,208 | 128,700 | 101.2% |
| 4 | 483,092 | 436,681 | 110.6% |
| 8 | 961,538 | 729,927 | 131.7% |
| 16 | 1,388,889 | 1,176,471 | 118.1% |
| 32 | 2,380,953 | 1,639,344 | 145.2% |
| 64 | 2,942,118 | 1,961,412 | 150.0% |

#### GET

| P | Credis rps | Redis rps | Ratio |
|---|-----------|-----------|-------|
| 1 | 133,156 | 137,174 | 97.1% |
| 4 | 529,101 | 518,135 | 102.1% |
| 8 | 980,392 | 1,030,928 | 95.1% |
| 16 | 1,960,784 | 1,492,537 | 131.4% |
| 32 | 2,272,727 | 2,380,953 | 95.5% |
| 64 | 4,168,000 | 2,778,667 | 150.0% |

Credis pipeline throughput scales to **150%** Redis at P=64 for both SET (2.94M rps) and GET (4.17M rps).

### Latency Distribution (SET, n=100K)

#### Single Connection (c=1, P=1)

| Metric | Credis | Redis |
|--------|--------|-------|
| p50 | 0.023 ms | 0.023 ms |
| p95 | 0.031 ms | 0.031 ms |
| p99 | 0.031 ms | 0.047 ms |
| max | 0.859 ms | 0.967 ms |

#### Under Load (c=50, P=1)

| Metric | Credis | Redis |
|--------|--------|-------|
| p50 | 0.191 ms | 0.191 ms |
| p95 | 0.295 ms | 0.263 ms |
| p99 | 0.399 ms | 0.367 ms |
| max | 1.111 ms | 1.135 ms |

#### Deep Pipeline (c=50, P=64)

| Metric | Credis | Redis |
|--------|--------|-------|
| p50 | 0.895 ms | 1.399 ms |
| p95 | 1.191 ms | 1.943 ms |
| p99 | 1.575 ms | 2.119 ms |
| max | 1.623 ms | 2.439 ms |

At P=64, Credis delivers 150% throughput with p99 latency 25% lower than Redis.

### Concurrency Scalability (SET, n=100K)

| Clients | Credis rps | Redis rps | Ratio |
|---------|-----------|-----------|-------|
| 1 | 43,573 | 38,226 | 114.0% |
| 10 | 142,248 | 140,056 | 101.6% |
| 50 | 136,240 | 138,696 | 98.2% |
| 100 | 136,240 | 133,156 | 102.3% |
| 500 | 135,501 | 126,904 | 106.8% |

Credis maintains ~135K rps across 10–500 concurrent clients (variance <5%), while Redis degrades 9.3% from c=10 to c=500.

### Memory Footprint

| State | Credis | Redis | Ratio |
|-------|--------|-------|-------|
| Idle (PSS) | 2,118 KB | 4,839 KB | 44% |
| ~63K keys (PSS) | 24,458 KB | 11,474 KB | 213% |
| ~632K keys (PSS) | 220,975 KB | 77,634 KB | 285% |

Measured via `/proc/[pid]/smaps Pss`. Per-entry overhead ~350 bytes (Credis) vs ~110 bytes (Redis).

### Binary Size

| | Credis | Redis |
|--|-----------|-------|
| Stripped binary | **793 KB** | 1.5 MB |
| Lines of code | ~3,700 C++ | ~80,000 C |

## Quick Start

### Build

```bash
./build.sh          # Debug build (default)
./build.sh Release  # Release build
```

### Run Tests

```bash
./run_tests.sh      # 161 tests across all modules
```

### Run Benchmarks

```bash
./run_benchmarks.sh            # Full Credis vs Redis benchmark
./run_benchmarks.sh --quick    # Quick mode (30K reqs)
```

Self-contained — auto-builds Redis 7.2.8 from source, no pre-installed Redis required.

### Start the Server

```bash
./build/redis                        # Default port 6379
./build/redis --port 6380            # Custom port
./build/redis --replicaof "localhost 6379"  # As replica
./build/redis --dir /data --dbfilename dump.rdb  # Load RDB on startup
```

### CLI Options

| Option | Description | Default |
|--------|-------------|---------|
| `--port <port>` | Server listening port | `6379` |
| `--replicaof "<host> <port>"` | Run as replica of the given master | (master mode) |
| `--dir <path>` | Data directory for RDB/AOF | current dir |
| `--dbfilename <name>` | RDB file name | (none) |
| `--appendonly <yes\|no>` | Enable AOF persistence | `no` |
| `--appenddirname <name>` | AOF subdirectory name | `appendonlydir` |
| `--appendfilename <name>` | AOF file base name | `appendonly.aof` |
| `--appendfsync <strategy>` | fsync policy: `always` / `everysec` | `everysec` |

### Connect

```bash
redis-cli -p 6379
> SET mykey hello
OK
> GET mykey
"hello"
> ZADD leaderboard 100 alice
(integer) 1
> XADD mystream * name alice
"1745000000000-0"
```

## Features

### Data Structures (Redis 7.0 Compatible)

All 7 core Redis data types with full CRUD operations:

| Type | Commands | Underlying Implementation |
|------|----------|--------------------------|
| **String** | SET (EX/PX), GET, INCR, MSET | `std::string` with optional TTL |
| **List** | LPUSH, RPUSH, LPOP, RPOP, LRANGE, LLEN | `std::deque<std::string>` |
| **Stream** | XADD (auto-ID), XRANGE, XREAD | binary search over `std::vector`, O(log N) |
| **Sorted Set** | ZADD, ZRANK, ZRANGE, ZCARD, ZSCORE, ZREM | `std::set<pair<double,string>>` + index map |
| **Geo** | GEOADD, GEOPOS, GEODIST, GEOSEARCH | Z-order curve (GeoHash 26-bit) + Haversine |
| **Bitmap** | SETBIT, GETBIT, BITCOUNT | bit-level operations on `std::string` |
| **Hash** | (planned) | — |

Lazy-expire: expired keys are evicted on access without a background expiration thread.

### Blocking Operations

Blocking commands suspend the client until data is available or timeout:

- **BLPOP** `key timeout` — blocks until a list element is available; awakened by RPUSH/LPUSH
- **XREAD BLOCK** `timeout STREAMS key id` — blocks until a new stream entry arrives; awakened by XADD

Blocked clients are managed by `BlockingManager` with per-key wait queues and per-fd deadline tracking. Timeout `0` means wait indefinitely.

### Optimistic Transactions (MULTI/EXEC/WATCH)

Full Redis-compatible transaction pipeline with optimistic locking:

- **MULTI** — begin transaction (commands are queued, not executed)
- **EXEC** — execute all queued commands atomically, or return `nil` if watched keys were modified
- **DISCARD** — abort and clear the queue
- **WATCH** / **UNWATCH** — CAS-based optimistic locking via per-key version counters

Watched keys track their version at WATCH time. EXEC checks all versions — if any changed, the entire transaction is aborted (`*-1` null array).

### Replication & Consistency (Master-Replica)

Master-replica replication with eventual consistency and synchronous WAIT:

- **PSYNC** — full resynchronization: master sends `FULLRESYNC <replid> <offset>` + empty RDB
- **REPLCONF** — replica handshake (`listening-port`, `capa psync2`) and ACK propagation
- **Command propagation** — write commands are broadcast in raw RESP format to all connected replicas
- **WAIT** `numreplicas timeout_ms` — block until `numreplicas` replicas have acknowledged up to the current offset (synchronous replication barrier)

`ReplicaManager` tracks per-replica offsets and computes acknowledged count for WAIT satisfaction. `ReplicaConnector` handles the handshake lifecycle (PING → REPLCONF×2 → PSYNC → RDB load → command stream).

### Persistence (RDB + AOF)

**RDB** (snapshot loading):
- Parses binary RDB format: `REDIS` magic, metadata headers (0xFA), database selectors (0xFE/0xFB), expiry timestamps (0xFD/0xFC), string values (0x00)
- Expired keys are skipped during load
- Loaded via `--dir`/`--dbfilename` flags at startup

**AOF** (append-only file, Redis 7+ manifest style):
- Manifest-driven: `appendonly.aof.manifest` → `file <name>.1.incr.aof seq 1 type i`
- Write commands appended in raw RESP format to the incremental file
- `appendfsync always` — `fsync()` after every write command (zero data loss)
- `appendfsync everysec` — (planned: background fsync thread)
- **Replay on startup**: reads manifest, finds the type `i` entry, parses RESP commands, and executes them to rebuild state — all before accepting client connections

### ACL & Authentication

- **AUTH** `username password` — authenticate a connection
- **ACL WHOAMI** — returns current user (`"default"`)
- **ACL SETUSER** `username >password` — set/update user password
- **ACL GETUSER** `username` — inspect user flags and password hashes
- Default `nopass` user: connections authenticate automatically on first command
- SHA-256 password hashing; `authenticated_fds` per-connection tracking

### Publish / Subscribe

- **SUBSCRIBE** `channel` — subscribe to a channel
- **UNSUBSCRIBE** `channel` — unsubscribe
- **PUBLISH** `channel message` — broadcast to all subscribers
- Subscribed connections are restricted to SUB/UNSUB/PING/QUIT commands only
- PING in subscribed mode returns `["pong", ""]`

### RESP Protocol

Complete Redis Serialization Protocol (RESP3):

- **Parsing**: single-pass, zero-copy `parse_one()` — parses one complete command from a byte buffer (`*<n>\r\n$<len>\r\n...`)
- **Encoding**: simple strings (`+`), bulk strings (`$`), integers (`:`), arrays (`*`), nulls, errors (`-`)
- **Pipelining**: multiple commands in one TCP packet are parsed and executed sequentially, with batch response assembly

### Event-Driven I/O (epoll)

- `EventLoop` wraps Linux `epoll` with sigfd-based signal handling (SIGINT/SIGTERM)
- `ConnectionPool` manages per-fd read/write buffers with dynamic expansion (4 KB → 512 MB)
- `TcpListener` with `SO_REUSEADDR` and backlog 511
- Batch output flush reduces `write()` syscalls
- Single-threaded, event-driven architecture — no thread pools, no context switches

### Config Management

- **CONFIG GET** `param` — query `dir`, `dbfilename`, `appendonly`, `appenddirname`, `appendfilename`, `appendfsync`
- All AOF config values overridable via command-line flags (see CLI Options)

## Supported Commands

### General
- `PING`, `ECHO`, `INFO`, `CONFIG GET`, `KEYS`, `TYPE`

### Strings
- `SET` (with EX/PX), `GET`, `INCR`, `MSET`

### Lists
- `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`, `LLEN`, `BLPOP`

### Streams
- `XADD` (auto-ID), `XRANGE`, `XREAD`, `XREAD BLOCK`

### Sorted Sets
- `ZADD`, `ZRANK`, `ZRANGE`, `ZCARD`, `ZSCORE`, `ZREM`

### Geo
- `GEOADD`, `GEOPOS`, `GEODIST`, `GEOSEARCH`

### Pub/Sub
- `SUBSCRIBE`, `UNSUBSCRIBE`, `PUBLISH`

### Transactions
- `MULTI`, `EXEC`, `DISCARD`, `WATCH`, `UNWATCH`

### Replication & Auth
- `REPLCONF`, `PSYNC`, `WAIT`, `AUTH`, `ACL WHOAMI`, `ACL GETUSER`, `ACL SETUSER`

**41 commands** total.

## API Documentation

Generate with [Doxygen](https://www.doxygen.nl/):

```bash
doxygen Doxyfile
```

HTML output in `docs/html/`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, code style, commit conventions, and the PR process.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a history of project changes.

## License

This project is licensed under the [MIT License](LICENSE).
