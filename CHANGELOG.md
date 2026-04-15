# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-04-15

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
