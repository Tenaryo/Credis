# Credis vs Redis Benchmark Report

**Date**: 2026-06-12 00:21:02
**Mode**: Full (100000 reqs/bench)
**Pipeline reqs**: 200000
**Redis**: port 6379
**Credis**: port 6380

## 1. Non-Pipeline Throughput

n=100000 per benchmark

### SET

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   38,226.3 |   43,573.0 | 114.0% | **Credis** |
| 10 |  140,056.0 |  142,247.5 | 101.6% | **Credis** |
| 50 |  138,696.2 |  136,239.8 |  98.2% | Redis |
| 100 |  133,155.8 |  136,239.8 | 102.3% | **Credis** |
| 500 |  126,903.6 |  135,501.4 | 106.8% | **Credis** |

### GET

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   39,261.9 |   45,351.5 | 115.5% | **Credis** |
| 10 |  134,408.6 |  134,952.8 | 100.4% | **Credis** |
| 50 |  136,798.9 |  141,844.0 | 103.7% | **Credis** |
| 100 |  135,318.0 |  136,798.9 | 101.1% | **Credis** |
| 500 |  123,762.4 |  126,582.3 | 102.3% | **Credis** |

### INCR

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   39,904.2 |   42,973.8 | 107.7% | **Credis** |
| 10 |  141,844.0 |  139,082.1 |  98.1% | Redis |
| 50 |  141,043.7 |  134,228.2 |  95.2% | Redis |
| 100 |  139,664.8 |  136,986.3 |  98.1% | Redis |
| 500 |  122,549.0 |  129,032.3 | 105.3% | **Credis** |

### LPUSH

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   37,299.5 |   42,158.5 | 113.0% | **Credis** |
| 10 |  136,612.0 |  140,845.1 | 103.1% | **Credis** |
| 50 |  128,700.1 |  133,333.3 | 103.6% | **Credis** |
| 100 |  138,312.6 |  137,931.0 |  99.7% | Redis |
| 500 |  119,760.5 |  120,048.0 | 100.2% | **Credis** |

### LPOP

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   38,066.2 |   44,943.8 | 118.1% | **Credis** |
| 10 |  143,061.5 |  132,450.3 |  92.6% | Redis |
| 50 |  126,262.6 |  131,579.0 | 104.2% | **Credis** |
| 100 |  134,770.9 |  132,802.1 |  98.5% | Redis |
| 500 |  115,473.4 |  124,223.6 | 107.6% | **Credis** |

### ZADD

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   36,900.4 |   41,753.7 | 113.2% | **Credis** |
| 10 |  124,843.9 |  129,870.1 | 104.0% | **Credis** |
| 50 |  131,752.3 |  130,208.3 |  98.8% | Redis |
| 100 |  135,501.4 |  132,275.1 |  97.6% | Redis |
| 500 |  107,874.9 |  124,069.5 | 115.0% | **Credis** |

### MSET

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   34,506.6 |   39,323.6 | 114.0% | **Credis** |
| 10 |  129,366.1 |  139,860.1 | 108.1% | **Credis** |
| 50 |  127,226.5 |  134,048.3 | 105.4% | **Credis** |
| 100 |  135,135.1 |  137,931.0 | 102.1% | **Credis** |
| 500 |  123,152.7 |  128,534.7 | 104.4% | **Credis** |

### LRANGE

| Concurrency | Redis rps | Credis rps | Ratio | Winner |
|------------:|----------:|-----------:|------:|--------|
| 1 |   38,037.3 |   42,771.6 | 112.4% | **Credis** |
| 10 |  142,653.4 |  136,054.4 |  95.4% | Redis |
| 50 |  129,870.1 |  138,696.2 | 106.8% | **Credis** |
| 100 |  134,952.8 |  139,664.8 | 103.5% | **Credis** |
| 500 |  123,609.4 |  124,378.1 | 100.6% | **Credis** |

## 2. Pipeline Throughput (c=50)

n=200000 per benchmark

### SET

| P | Redis rps | Credis rps | Ratio | Winner |
|---:|----------:|-----------:|------:|--------|
| 1 |  128,700.1 |  130,208.3 | 101.2% | **Credis** |
| 4 |  436,681.2 |  483,091.8 | 110.6% | **Credis** |
| 8 |  729,927.1 |  961,538.4 | 131.7% | **Credis** |
| 16 | 1,176,470.6 | 1,388,889.0 | 118.1% | **Credis** |
| 32 | 1,639,344.2 | 2,380,952.5 | 145.2% | **Credis** |
| 64 | 1,961,411.8 | 2,942,117.5 | 150.0% | **Credis** |

### GET

| P | Redis rps | Credis rps | Ratio | Winner |
|---:|----------:|-----------:|------:|--------|
| 1 |  137,174.2 |  133,155.8 |  97.1% | Redis |
| 4 |  518,134.7 |  529,100.6 | 102.1% | **Credis** |
| 8 | 1,030,927.8 |  980,392.2 |  95.1% | Redis |
| 16 | 1,492,537.2 | 1,960,784.4 | 131.4% | **Credis** |
| 32 | 2,380,952.5 | 2,272,727.2 |  95.5% | Redis |
| 64 | 2,778,666.8 | 4,168,000.0 | 150.0% | **Credis** |

## 3. Latency Distribution (SET, n=100K)

### c=1

| Server | P | p50 (ms) | p95 (ms) | p99 (ms) | max (ms) | rps |
|--------|---|---------:|---------:|---------:|---------:|----:|
| Redis | 1 | 0.023 | 0.031 | 0.047 | 0.967 | 37,509 |
| Redis | 64 | 0.063 | 0.071 | 0.191 | 0.367 | 926,222 |
| Credis | 1 | 0.023 | 0.031 | 0.031 | 2.159 | 42,445 |
| Credis | 64 | 0.031 | 0.039 | 0.047 | 0.143 | 1,961,412 |

### c=50

| Server | P | p50 (ms) | p95 (ms) | p99 (ms) | max (ms) | rps |
|--------|---|---------:|---------:|---------:|---------:|----:|
| Redis | 1 | 0.191 | 0.263 | 0.367 | 1.135 | 134,590 |
| Redis | 64 | 1.399 | 1.943 | 2.119 | 2.439 | 2,000,640 |
| Credis | 1 | 0.191 | 0.295 | 0.399 | 1.111 | 127,877 |
| Credis | 64 | 0.895 | 1.191 | 1.575 | 1.623 | 2,942,118 |


## 4. Perf Stat (PGO Credis, SET+GET, c=50)

### P=1 (non-pipeline)
```
  task-clock: 2.946B
  IPC (core): 1.58
  Branch miss: 0.75%
  L1 miss: 3.28%
  Time: 2.96s  (user: 0.34s, sys: 2.61s, sys ratio: 88.2%)
```

### P=8 (pipeline)
```
  task-clock: 0.413B
  IPC (core): 1.70
  Branch miss: 0.66%
  L1 miss: 2.92%
  Time: 0.42s  (user: 0.08s, sys: 0.34s, sys ratio: 81.0%)
```

### P=64 (deep pipeline)
```
  task-clock: 0.090B
  IPC (core): 2.38
  Branch miss: 0.36%
  L1 miss: 1.78%
  Time: 0.13s  (user: 0.03s, sys: 0.06s, sys ratio: 45.3%)
```

## 5. Build Configuration

- Compiler: GCC 15.2.0
- Flags: `-O3 -march=native -fprofile-use`
- PGO training: 200K SET/GET/INCR/LPUSH/LPOP/ZADD/MSET + P=8,P=64 pipeline + c=500

## 6. Optimizations Applied

1. **Batch output flush** — `Connection::send_data` queues responses, `EventLoop::run` flushes after each iteration
2. **Single-pass RESP parsing** — Replaced `string_view::find("\r\n")` + `from_chars` with hand-rolled `parse_int_until_crlf`

Source: `src/connection/connection.{hpp,cpp}`, `src/connection/connection_pool.{hpp,cpp}`, `src/event_loop/event_loop.{hpp,cpp}`, `src/server/event_dispatch.cpp`, `src/main.cpp`, `src/protocol/resp_codec.cpp`
