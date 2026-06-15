# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-06-15

### Added
- **AOF persistence**: `--appendonly`, `--appenddirname`, `--appendfilename`, `--appendfsync` CLI flags
  — write commands appended in raw RESP format to incremental AOF file
  — `appendfsync always` — `fsync()` after every write command (zero data loss)
  — `appendfsync everysec` — background fsync thread (planned)
  — manifest-driven (Redis 7+ style): `appendonly.aof.manifest` → `file <name>.1.incr.aof seq 1 type i`
  — startup replay: reads manifest, parses RESP commands from AOF, executes to rebuild state
- AOF multi-command append integration test
- Reset benchmark scripts (Credis vs Redis 7.2.8, auto-buildable)

### Changed
- **RESP parser**: replaced `find("\r\n")` + `from_chars` with hand-written single-pass `parse_int_until_crlf`
- **Connection**: batch output flush via `pending_write_` buffer + `flush_all()` — reduces `write()` syscalls
- Benchmark suite refactored: warmup support, median-of-N reporting, random-order execution, seed reproducibility
- Removed codecrafters files
- Removed Memory Footprint and Binary Size sections from README

### Fixed
- Check `write()`/`fsync()` return values in `AofManager::append`
- Benchmark latency parsing crash on malformed output
- Restore missing `sys` import in benchmark runner

## [1.1.0] - 2026-05-30

### Added
- `RPOP` list operation (single-element and counted variants)
- `MSET` bulk string set command
- ZADD multi score/member pair support
- RDB `load_into_store` convenience function
- Piped response batching in `process_with_fd` for pipeline throughput
- Store `mset` bulk interface for efficient multi-key writes
- PGO build support via `ENABLE_PGO` CMake option
- Pre-commit hooks with clang-format and clang-tidy (non-blocking)

### Changed
- **RESP parser**: `parse_one` now returns `vector<string_view>` args (zero-copy)
  — full `std::string` copy deferred to MULTI queuing path
- **SortedSet**: three-round micro-architecture optimization
  — transparent hashing (`find(string_view)` zero-alloc)
  — map stores set iterator (O(1) ZREM, ZRANK single lookup)
  — `string_view` key + `set::extract` (single string copy, zero-copy updates)
- **ReplicaManager**: three hash tables replaced with single `vector<ReplicaState>`
  — eliminated dual buffer via `process_ack` returning consumed count
  — `master_repl_offset` moved to `offset_fn` callback
- **Store**: `find_valid_entry` and `get_or_create_typed` cleaned
  — `keys()`/`lrange()` vectors pre-reserved
  — Stream `parsed_id` cached in `StreamEntry` to eliminate parse in binary search
- **CommandHandler**: 9 duplicated fields consolidated into single `CommandContext`
  — `send_to_client` passed by const-ref
- **EventLoop**: signal handling internalized (SIGINT/SIGTERM registration), `g_loop` eliminated
- **Build**: Release adds `-march=native` + LTO
- **Utilities**: `to_upper`/`to_lower` accept `string_view`
- README benchmark section fully updated with PGO results
- Test count: 144 (up from 138)

### Fixed
- `lpush_with_blocking` missing `lpush` on wake path (value never entered list)
- Replica connection buffer leak (missing `consume` in dispatch)
- `replica_config.master_repl_offset` dead field replaced with live callback

## [1.0.0] - 2026-04-15

### Added

#### Core Server
- TCP server with epoll-based event loop
- RESP v2 protocol parser and encoder
- Support for concurrent client connections
- Command-line argument parser (`--port`, `--replicaof`, `--dir`, `--dbfilename`)

#### String Commands
- `SET` with optional TTL (`EX` seconds, `PX` milliseconds)
- `GET`
- `INCR` (atomic integer increment)

#### List Commands
- `LPUSH` / `RPUSH` (with blocking wake support)
- `LPOP` (with optional count)
- `LRANGE` (with negative indexing)
- `LLEN`
- `BLPOP` (blocking pop with timeout)

#### Stream Commands
- `XADD` with automatic ID generation (`*`), auto-incrementing sequences (`<ts>-*`), and explicit IDs
- `XRANGE` with special ID support (`-`, `+`)
- `XREAD` with blocking support and `$` ID for new entries

#### Sorted Set Commands
- `ZADD` (add/update member with score)
- `ZRANK` (get member rank)
- `ZRANGE` (range query by rank, with negative index support)
- `ZCARD` (cardinality)
- `ZSCORE` (score retrieval with full double precision)
- `ZREM` (member removal)

#### Geo Commands
- `GEOADD` (with latitude/longitude validation and geohash encoding)
- `GEOPOS` (coordinate decoding from geohash)
- `GEODIST` (Haversine distance calculation)
- `GEOSEARCH` (FROMLONLAT + BYRADIUS with M/KM/MI/FT units)

#### Pub/Sub
- `SUBSCRIBE` / `UNSUBSCRIBE`
- `PUBLISH` (message delivery to all subscribers)
- Subscribed mode command filtering

#### Transactions
- `MULTI` / `EXEC` / `DISCARD`
- `WATCH` (optimistic locking with key version tracking)
- `UNWATCH` (clear watched keys)

#### Replication
- Full replication handshake (PING → REPLCONF → PSYNC)
- Master-replica command propagation
- `WAIT` command for replica acknowledgment
- `REPLCONF GETACK` offset tracking
- Empty RDB file transfer for full resynchronization
- Graceful handling of master disconnect

#### ACL & Authentication
- `AUTH` command with SHA-256 password verification
- `ACL WHOAMI`
- `ACL GETUSER` (with flags and password hashes)
- `ACL SETUSER` (password setting with SHA-256 hashing)
- Per-connection authentication state enforcement

#### General Commands
- `PING` (with subscribed mode support)
- `ECHO`
- `INFO` (replication section with role, master_replid, master_repl_offset)
- `CONFIG GET` (`dir`, `dbfilename`)
- `KEYS` (with lazy expiration)
- `TYPE`

#### Infrastructure
- RDB file parser for loading snapshots on startup
- Test framework with 31 test executables
- Static library build for faster compilation
- Precompiled headers (PCH) support
- Ninja build system integration

### Changed

- Extracted `RedisApp` class from `main.cpp` for cleaner architecture
- Moved command dispatch to `CommandHandler` with dispatch table
- Replaced `std::stod` with `std::from_chars` for zero-allocation double parsing
- Replaced `inet_pton` with `getaddrinfo` for hostname resolution support
- Upgraded to `-O3` release builds with `-Wshadow -Wconversion` warnings
- Extracted `StringHash` utility, `StreamEntry`, heterogeneous lookup
- Deduplicated block client and replica buffer processing
- Introduced `ProcessResult` variant and template callback system
- Extracted `kEmptyRdb` to rdb module

### Fixed

- Stream ID auto-generation bug
- XRANGE handling of special IDs (`-`, `+`)
- `CommandHandler::store_` dangling reference
- Event loop crash on master disconnect in replica mode
- ZSCORE double precision loss (using `%.17g` format)
- GEOADD field validation
- Safe EXEC variant access
- XADD field validation
