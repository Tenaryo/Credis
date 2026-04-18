# Credis

[![CI](https://github.com/Tenaryo/Credis/actions/workflows/ci.yml/badge.svg)](https://github.com/Tenaryo/Credis/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

Redis server implementation in C++23 (3,600 LOC, zero dependencies), featuring an epoll-based event loop, RESP protocol support, master-replica replication, Pub/Sub, Streams, Sorted Sets, Geo commands, ACL authentication and 39 Redis commands across 10 modules.

## Table of Contents

- [Features](#features)
  - [Data Types](#data-types)
  - [Supported Commands](#supported-commands)
- [Architecture](#architecture)
- [Key Design Decisions](#key-design-decisions)
- [Usage](#usage)
  - [Build](#build)
  - [Run](#run)
  - [CLI Options](#cli-options)
  - [Connect with redis-cli](#connect-with-redis-cli)
- [Testing](#testing)
- [Replication](#replication)
- [Performance](#performance)
- [API Documentation](#api-documentation)
- [Contributing](#contributing)
- [Changelog](#changelog)
- [License](#license)

## Features

### Data Types

| Type | Description |
|------|-------------|
| `string` | Basic key-value with TTL support (`EX`/`PX`) |
| `list` | Doubly-linked list (deque-based) with blocking operations |
| `stream` | Append-only log with time-series IDs |
| `zset` | Sorted set with score-based ordering |

### Supported Commands

#### General

| Command | Description |
|---------|-------------|
| `PING` | Test connectivity (supports subscribed mode) |
| `ECHO` | Echo the given string |
| `INFO` | Server and replication info |
| `CONFIG GET` | Get configuration parameters (`dir`, `dbfilename`) |
| `KEYS` | List all stored keys (triggers lazy expiration) |
| `TYPE` | Get the type of a key |

#### String Operations

| Command | Description |
|---------|-------------|
| `SET key value [EX seconds] [PX milliseconds]` | Set a key-value pair with optional TTL |
| `GET key` | Get the value of a key |
| `INCR key` | Atomically increment an integer value |

#### List Operations

| Command | Description |
|---------|-------------|
| `LPUSH key value [value ...]` | Push one or more values to the head |
| `RPUSH key value [value ...]` | Push one or more values to the tail |
| `LPOP key [count]` | Pop one or more values from the head |
| `LRANGE key start stop` | Get a range of elements |
| `LLEN key` | Get the length of a list |
| `BLPOP key timeout` | Blocking pop from the head |

#### Stream Operations

| Command | Description |
|---------|-------------|
| `XADD key ID field value [field value ...]` | Add an entry to a stream |
| `XRANGE key start end` | Query a range of entries |
| `XREAD [BLOCK ms] STREAMS key ID` | Read entries after a given ID |
| `XREAD [BLOCK ms] STREAMS key $` | Block until new entries arrive |

The `XADD` command supports automatic ID generation (`*`), auto-incrementing sequences (`<timestamp>-*`), and explicit IDs.

#### Sorted Set Operations

| Command | Description |
|---------|-------------|
| `ZADD key score member` | Add or update a member with score |
| `ZRANK key member` | Get the rank of a member |
| `ZRANGE key start stop` | Query a range by rank (supports negative indices) |
| `ZCARD key` | Get the number of members |
| `ZSCORE key member` | Get the score of a member |
| `ZREM key member` | Remove a member |

#### Geo Operations

| Command | Description |
|---------|-------------|
| `GEOADD key longitude latitude member` | Add a geospatial item (validates lon/lat) |
| `GEOPOS key member [member ...]` | Get coordinates of members |
| `GEODIST key member1 member2` | Haversine distance between two members |
| `GEOSEARCH key FROMLONLAT lon lat BYRADIUS radius unit` | Search within radius (M/KM/MI/FT) |

#### Pub/Sub

| Command | Description |
|---------|-------------|
| `SUBSCRIBE channel` | Subscribe to a channel |
| `UNSUBSCRIBE channel` | Unsubscribe from a channel |
| `PUBLISH channel message` | Publish a message to a channel |

Subscribed clients can only execute `SUBSCRIBE`, `UNSUBSCRIBE`, `PING`, `QUIT`, and `RESET`.

#### Transactions

| Command | Description |
|---------|-------------|
| `MULTI` | Start a transaction |
| `EXEC` | Execute all queued commands (with optimistic locking) |
| `DISCARD` | Discard the transaction |
| `WATCH key [key ...]` | Watch keys for optimistic concurrency control |
| `UNWATCH` | Clear all watched keys |

#### Replication

| Command | Description |
|---------|-------------|
| `REPLCONF` | Replica configuration handshake |
| `PSYNC` | Partial/full synchronization with master |
| `WAIT numreplicas timeout` | Block until N replicas acknowledge writes |

#### ACL & Authentication

| Command | Description |
|---------|-------------|
| `AUTH username password` | Authenticate with username and password |
| `ACL WHOAMI` | Return the current authenticated username |
| `ACL GETUSER username` | Get user flags and password hashes |
| `ACL SETUSER username >password` | Set password for a user (SHA-256 hashed) |

## Architecture

```
src/
  cli/              Command-line argument parser
  connection/       TCP connection abstraction (read/write buffers)
  event_loop/       epoll-based I/O event loop
  handler/          Command routing and execution
  protocol/         RESP protocol parser and encoder
  rdb/              RDB file parser (loads snapshots on startup)
  replica/          Master-replica handshake and command propagation
  server/           TCP server socket, server config, application runner, ACL manager
  store/            In-memory data store (String, List, Stream, SortedSet)
  block_manager/    Blocking operation manager (BLPOP, XREAD BLOCK)
  pubsub/           Pub/Sub channel manager
  geo/              Geohash encoding/decoding and Haversine distance
  util/             Integer parsing, string utilities, SHA-256
```

### Key Design Decisions

- **epoll event loop** -- All I/O is handled through a single-threaded epoll loop with configurable timeouts for blocking operations and WAIT.
- **Lazy expiration** -- Keys with TTL are checked for expiration on access (`find_valid_entry`) and periodically cleaned during `KEYS` calls.
- **RESP v2** -- Full parser for the Redis Serialization Protocol, supporting inline parsing of pipelined commands via `parse_one`.
- **Streaming replication** -- Write commands are automatically propagated to connected replicas. The `WAIT` command tracks replica acknowledgment offsets.
- **Optimistic locking** -- `WATCH` tracks key versions; `EXEC` aborts if any watched key was modified.
- **Zero dependencies** -- Uses only the C++23 standard library and POSIX APIs (epoll, sockets). No third-party libraries required.

## Usage

### Build

```bash
./build.sh          # Debug build (default)
./build.sh Release  # Release build
```

Requires:
- C++23 compiler (GCC 13+ recommended)
- CMake 3.21+
- Ninja (optional, recommended)

### Run

```bash
# Start a server on default port 6379
./build/redis

# Custom port
./build/redis --port 6380

# Start as replica of a master
./build/redis --port 6380 --replicaof "localhost 6379"

# Load an RDB file on startup
./build/redis --dir /data --dbfilename dump.rdb
```

### CLI Options

| Option | Description | Default |
|--------|-------------|---------|
| `--port <port>` | Server listening port | `6379` |
| `--replicaof "<host> <port>"` | Run as replica of the given master | (master mode) |
| `--dir <path>` | Directory containing the RDB file | (none) |
| `--dbfilename <name>` | RDB file name to load | (none) |

### Connect with redis-cli

```bash
redis-cli -p 6379
> SET mykey hello
OK
> GET mykey
"hello"
> SET tempkey value EX 10
OK
> INCR counter
(integer) 1
> LPUSH mylist a b c
(integer) 3
> LRANGE mylist 0 -1
1) "c"
2) "b"
3) "a"
> ZADD leaderboard 100 alice 85 bob
(integer) 2
> ZRANGE leaderboard 0 -1
1) "bob"
2) "alice"
> GEOADD cities 13.361389 38.115556 "Palermo"
(integer) 1
> GEODIST cities Palermo Catania
"166.27"
> XADD mystream * name alice age 30
"1745000000000-0"
> XRANGE mystream - +
1) 1) "1745000000000-0"
   2) 1) "name"
      2) "alice"
      3) "age"
      4) "30"
```

## Testing

```bash
./run_tests.sh
```

Tests are built alongside the main binary. Each test file in `tests/` is compiled as a standalone executable and exercises specific subsystems:

- **Commands**: SET, GET, INCR, KEYS, TYPE, CONFIG GET, INFO
- **List operations**: LPUSH, RPUSH, LPOP, LRANGE, LLEN, BLPOP
- **Stream operations**: XADD (auto-ID, auto-seq), XRANGE, XREAD, XREAD BLOCK, XREAD `$`
- **Sorted set operations**: ZADD, ZRANK, ZRANGE, ZCARD, ZSCORE, ZREM
- **Geo operations**: GEOADD (with validation)
- **Pub/Sub**: SUBSCRIBE, PUBLISH, UNSUBSCRIBE
- **Transactions**: MULTI, EXEC, DISCARD, WATCH, UNWATCH
- **ACL & Auth**: AUTH, ACL WHOAMI, ACL GETUSER, ACL SETUSER
- **Replication**: handshake (PING, REPLCONF, PSYNC), command propagation, WAIT
- **RDB parsing**
- **CLI argument parsing**

## Replication

Credis supports master-replica replication:

1. **Master** starts normally, accepting connections.
2. **Replica** starts with `--replicaof "<master-host> <master-port>"` and performs a full handshake (PING -> REPLCONF -> PSYNC).
3. Write commands executed on the master are automatically propagated to all connected replicas.
4. Use `WAIT numreplicas timeout` to block until the specified number of replicas have acknowledged the writes.

## Performance

Benchmarked against Redis 6.0.16 using `redis-benchmark` (50 concurrent connections, 200K requests).

### Throughput vs Redis 6.0

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

Core String/List operations outperform Redis 6.0 by 2.7%~10.6%. Overall throughput is ~103% of Redis.

### Latency (single connection)

| Percentile | Credis | Redis 6.0 |
|------------|-----------|-----------|
| P50 | 0.024ms | 0.024ms |
| P99 | 0.110ms | 0.112ms |
| P99.9 | 0.142ms | 0.146ms |
| Max | 0.186ms | 0.299ms |

Tail latency is 38% lower than Redis (0.186ms vs 0.299ms max).

### Concurrency Scaling

| Connections | Credis | Redis 6.0 |
|-------------|-----------|-----------|
| 1 | 39,172 | 38,880 |
| 10 | 289,855 | 288,184 |
| 50 | 313,480 | 321,543 |
| 100 | 322,581 | 325,733 |
| 200 | 321,543 | 324,675 |

Near-identical scaling curve to Redis, saturating at ~322K ops/s.

### Binary Size

| | Credis | Redis 6.0 |
|--|-----------|-----------|
| Stripped binary | **291 KB** | 1.5 MB |

### Current Limitations

- No pipeline support (only first command per read is processed)
- Blocking write (`send_data` is synchronous)
- No persistence (RDB load only, no SAVE/BGSAVE/AOF)
- Single-threaded (no multi-core utilization)
- Missing DEL, Hash, Set types

## API Documentation

Generate API documentation with [Doxygen](https://www.doxygen.nl/):

```bash
doxygen Doxyfile
```

The HTML documentation will be generated in `docs/html/`. Open `docs/html/index.html` in a browser to view it.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, code style, commit conventions, and the PR process.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a history of project changes.

## License

This project is licensed under the [MIT License](LICENSE).
