# Matching Gateway

C++20 matching gateway that integrates **binary market-data ingest** with a **price-time order book**: market data and client orders in, trades and acks out, with deterministic session record/replay.

This is an **in-memory, single-threaded** educational stack (map-backed LOB, pooled orders). Numbers below are not exchange wire-to-book latency.

## Architecture

```
UDP / session recording
        │
        ├─ market data ──► mdf FeedHandler (parse, seq, gap/dup)
        │                         │
        │                         ▼
        │                  mirror Add/Cancel/Modify
        │                         │
        │                         ▼
        └─ client orders ──► MatchingGateway ──► Orderbook matcher
                                      │
                                      ▼
                               ExecReports (Ack / Trade / Reject / CancelAck)
```

- **Market data** uses the vendored `mdf` binary protocol and feed handler.
- Applied MD add/cancel/modify events are **mirrored** onto the matching LOB as resting GTC liquidity.
- **Client orders** use a compact `OR` wire protocol (new / cancel / modify) with GTC, FAK, FOK, GFD, Market.
- One shared `Orderbook` is the source of truth for fills.

## Build

```bash
cmake -G "MinGW Makefiles" -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
cmake --build build -j
```

| Target     | Description                       |
|------------|-----------------------------------|
| `gw`       | Gateway library                   |
| `gw_demo`  | Smoke / replay demo CLI          |
| `gw_tests` | Integration tests                 |
| `gw_bench` | Ingress-to-ack throughput/latency |

```bash
./build/gw_demo smoke
./build/gw_demo replay-demo
./build/gw_tests
./build/gw_bench
```

## Tests

7 GoogleTest cases:

- MD seeds liquidity, aggressive buy matches FIFO
- FOK miss rejects and leaves book unchanged
- Duplicate client order id rejected
- Duplicate MD dropped by seq tracker
- Out-of-order MD buffered then flushed
- Gap beyond OOO window detected
- Session record/replay identical trade log

## Benchmarks

Measured on a modern desktop (Release, prune off, locking off, `OrderPool`, in-memory):

| Path | Approx. |
|------|---------|
| Resting client orders | ~1.05M orders/s |
| Aggressive match (MD-seeded book) | ~0.57M orders/s |
| Ingress-to-ack latency | median ~700 ns, p99 ~3.4 µs |

## Layout

```
include/gw/          gateway, order protocol, session recorder
include/mdf/         vendored feed-handler headers
third_party/lob/     vendored price-time order book
src/                 gateway + mdf sources
tests/               GoogleTest
bench/               Google Benchmark
```

## License

Vendored components retain their upstream licenses. Add a top-level license before publishing.
