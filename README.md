# Credis

[![CI](https://github.com/Tenaryo/Credis/actions/workflows/ci.yml/badge.svg)](https://github.com/Tenaryo/Credis/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

A Redis 7.0-compatible server written from scratch in C++23. 3,600 lines, zero dependencies, 291 KB binary. Outperforms Redis 6.0 by up to 10.6% in throughput with 38% lower tail latency. Implements the full single-threaded epoll reactor, RESP v2 pipelining, master-replica replication with WAIT acknowledgment, blocking commands with O(1) dual-index wake queue, and optimistic locking transactions — all with no third-party libraries.

## Dependencies

- **C++23** compiler (GCC 13+ recommended)
- **CMake** 3.21+
- **POSIX** (epoll, sockets)

That's it. No Boost, no libevent, no hiredis.

## Quick Start

### Build

```bash
./build.sh          # Debug build (default)
./build.sh Release  # Release build
```

### Run Tests

```bash
./run_tests.sh      # 140 tests across all modules
```

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
| `--dir <path>` | Directory containing the RDB file | (none) |
| `--dbfilename <name>` | RDB file name to load | (none) |

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

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                             main.cpp                                │
│                     assemble & wire dependency                       │
└────────────────┬────────────────┬───────────────────────────────────┘
                 │                │
┌────────────────┘                └──────────────────────┐
v                                                         v
┌───────────────────────────┐    ┌───────────────────────────┐
│       event_dispatch      │    │    ReplicaConnector       │
│  glue: fd routing +       │    │  PING→REPLCONF→PSYNC→RDB │
│  result dispatch           │    └───────────────────────────┘
└──┬───┬───┬───┬───┬───┬───┘
   │   │   │   │   │   │
   v   v   v   v   v   v
┌──────┐ ┌──────┐ ┌────────┐ ┌───────────┐ ┌──────────┐ ┌───────────┐
│Event │ │Conn. │ │Command │ │Blocking   │ │PubSub    │ │Replica    │
│Loop  │ │Pool  │ │Handler │ │Manager    │ │Manager   │ │Manager    │
│epoll │ │fd→   │ │cmd     │ │BLPOP/     │ │SUB/PUB   │ │WAIT/      │
│wait  │ │Conn  │ │table   │ │XREAD BLK  │ │          │ │offset     │
└──┬───┘ └──┬───┘ └───┬────┘ └─────┬─────┘ └────┬─────┘ └─────┬─────┘
   │        │         │             │             │              │
   │   read/write     │        wake_client    push msg     ack tracking
   │        │         │             │             │              │
   │        v         v             │             │              │
   │   ┌────────┐ ┌───────┐         │             │              │
   │   │TCP fd  │ │ Store │◄────────┘             │              │
   │   │buffer  │ │key-val│                       │              │
   │   └────────┘ └───┬───┘                       │              │
   │                  │                           │              │
   │         ┌────────┼───────────────────────────┘              │
   │         │        │                                          │
   │    ┌────▼───┐ ┌──▼────┐                                     │
   │    │RespEnc │ │RespPar│                                     │
   │    │(+OK)   │ │(*3)   │                                     │
   │    └────┬───┘ └───────┘                                     │
   │         │                                                   │
   └─────────┼───────────────────────────────────────────────────┘
             │
             v
        ┌─────────┐
        │ Client  │
        └─────────┘
```

### Data Flow

```
Request (main path):
  Client → TCP → Connection.read() → RESP parse → CommandHandler
  → Store (CRUD) → RESP encode → Connection.send() → Client

Blocking (BLPOP / XREAD BLOCK):
  BLPOP → BlockingManager.block() → return ProcessResult::Block
  RPUSH → Store.rpush → BlockingManager.wake() → send to blocked fd

Pub/Sub:
  SUBSCRIBE → PubSubManager (fd→channels, channel→fds)
  PUBLISH → PubSubManager → send to each subscriber fd

Replication:
  Master: SET → propagate → ReplicaManager → send to all replica fds
  Replica: PING→REPLCONF→PSYNC→RDB → ReplicaConnector
  WAIT:    ReplicaManager.start_wait → process_ack → reply with count

Event Loop:
  epoll_wait → dispatch_event(fd) → 4-way branch:
    listener fd?  → accept → add_fd
    replica fd?   → process_ack
    master fd?    → process_propagated
    client fd?    → read → process → send → consume
```

### Modules

| Module | Role | Depends On |
|--------|------|------------|
| `event_loop/` | epoll-based I/O event loop. Injects `on_event` and `get_timeout` callbacks for zero dependency on business logic. | — |
| `connection/` | TCP connection abstraction with dynamic read buffer (auto-grow to 512 MB) and non-blocking send. `ConnectionPool` maps fd → Connection. | — |
| `store/` | In-memory data store. `Value = variant<String, List, Stream, SortedSet>`. Lazy expiration, optimistic locking (`version_counter_`) for WATCH/EXEC. | `util/` |
| `protocol/` | RESP v2 parser (`parse_one` with consumed tracking for pipelining) and encoder. | `store/` |
| `handler/` | Command routing. `CommandHandler` owns a `command_table_` (cmd → handler), dispatches via `execute_command`. Dependencies injected through `CommandContext`. | `store/`, `protocol/`, `blocking_manager/`, `pubsub/` |
| `blocking_manager/` | O(1) dual-index blocking queue for BLPOP and XREAD BLOCK. `fd↔key` bidirectional lookup with timeout management. | — |
| `pubsub/` | Pub/Sub channel manager with dual index (fd→channels, channel→fds) and subscribed-mode command restriction. | `util/` |
| `replica/` | `ReplicaManager` tracks per-replica acknowledgment offsets for WAIT. `ReplicaConnector` performs full handshake (PING→REPLCONF→PSYNC→RDB). | `protocol/`, `server/` |
| `rdb/` | RDB file parser supporting encoded strings, expire timestamps, and multiple key-value pairs. | `store/` |
| `geo/` | Geohash encoding/decoding and Haversine distance calculation for Geo commands. | — |
| `server/` | `TcpListener` (non-blocking socket), `ServerConfig`, `AclManager` (SHA-256 password auth), `event_dispatch` (I/O glue layer). | `handler/`, `connection/`, `event_loop/`, `replica/` |
| `cli/` | Command-line argument parser (`--port`, `--replicaof`, `--dir`, `--dbfilename`). | `server/` |
| `util/` | `Error` type, `Logger`, `SHA-256`, `parse_int<T>`/`parse_double`, transparent string hashing. | — |

### Key Design Decisions

- **epoll event loop** — All I/O is handled through a single-threaded epoll loop with configurable timeouts for blocking operations and WAIT.
- **Lazy expiration** — Keys with TTL are checked for expiration on access (`find_valid_entry`) and periodically cleaned during `KEYS` calls.
- **RESP v2** — Full parser for the Redis Serialization Protocol, supporting pipeline parsing via `parse_one` with consumed-byte tracking.
- **Streaming replication** — Write commands are automatically propagated to connected replicas. The `WAIT` command tracks replica acknowledgment offsets.
- **Optimistic locking** — `WATCH` tracks key versions; `EXEC` aborts if any watched key was modified.
- **Zero dependencies** — Uses only the C++23 standard library and POSIX APIs (epoll, sockets). No third-party libraries required.

## Supported Commands

### General
| Command | Description |
|---------|-------------|
| `PING`, `ECHO`, `INFO`, `CONFIG GET`, `KEYS`, `TYPE` | |

### Strings
| `SET` (with EX/PX), `GET`, `INCR`, `MSET` |

### Lists
| `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`, `LLEN`, `BLPOP` |

### Streams
| `XADD` (auto-ID), `XRANGE`, `XREAD`, `XREAD BLOCK` |

### Sorted Sets
| `ZADD`, `ZRANK`, `ZRANGE`, `ZCARD`, `ZSCORE`, `ZREM` |

### Geo
| `GEOADD`, `GEOPOS`, `GEODIST`, `GEOSEARCH` |

### Pub/Sub
| `SUBSCRIBE`, `UNSUBSCRIBE`, `PUBLISH` |

### Transactions
| `MULTI`, `EXEC`, `DISCARD`, `WATCH`, `UNWATCH` |

### Replication & Auth
| `REPLCONF`, `PSYNC`, `WAIT`, `AUTH`, `ACL WHOAMI`, `ACL GETUSER`, `ACL SETUSER` |

**41 commands** total. Full details in [Commands Reference](#).

## Performance

Benchmarked against Redis 6.0.16 using `redis-benchmark` (50 concurrent connections, 200K requests).

### Throughput

| Command | Credis | Redis 6.0 | Delta |
|---------|-----------|-----------|-------|
| SET | 312,989 | 303,030 | **+3.3%** |
| GET | 325,733 | 297,619 | **+9.4%** |
| INCR | 318,471 | 301,205 | **+5.7%** |
| LPUSH | 316,957 | 296,296 | **+7.0%** |
| LPOP | 297,619 | 289,855 | **+2.7%** |
| LRANGE_100 | 194,932 | 187,970 | **+3.7%** |
| LRANGE_300 | 87,873 | 79,428 | **+10.6%** |
| ZADD | 300,300 | 333,333 | -9.9% |

### Latency (single connection)

| Percentile | Credis | Redis 6.0 |
|------------|-----------|-----------|
| P50 | 0.024ms | 0.024ms |
| P99 | 0.110ms | 0.112ms |
| P99.9 | 0.142ms | 0.146ms |
| Max | 0.186ms | 0.299ms |

Tail latency is 38% lower than Redis (0.186ms vs 0.299ms max).

### Binary Size

| | Credis | Redis 6.0 |
|--|-----------|-----------|
| Stripped binary | **291 KB** | 1.5 MB |

## Limitations

- Blocking write (`send_data` is synchronous)
- No persistence (RDB load only, no SAVE/BGSAVE/AOF)
- Single-threaded (no multi-core utilization)
- Missing DEL, Hash, Set types
- Graceful shutdown is basic (SIGINT/SIGTERM stops loop; TODO: drain requests, flush RDB/AOF)

## Replication

1. **Master** starts normally, accepting connections.
2. **Replica** starts with `--replicaof "<host> <port>"` and performs a full handshake (PING → REPLCONF → PSYNC).
3. Write commands executed on the master are automatically propagated to all connected replicas.
4. Use `WAIT numreplicas timeout` to block until the specified number of replicas have acknowledged the writes.

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
