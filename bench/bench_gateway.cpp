#include "gw/gateway.hpp"
#include "gw/order_protocol.hpp"

#include "mdf/protocol.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace {

void seed_asks(gw::MatchingGateway& g, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    g.on_market_data(mdf::encode_add(
        static_cast<uint64_t>(i + 1), 1,
        mdf::AddMsg{static_cast<uint64_t>(i + 1), mdf::Side::Ask, 100, 1}));
  }
}

} // namespace

static void BM_RestingOrders(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    state.PauseTiming();
    gw::MatchingGateway g;
    std::vector<std::vector<std::byte>> orders;
    orders.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      orders.push_back(gw::encode_new_order(
          10'000 + i, 1,
          gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc,
                             static_cast<int32_t>(90 - (i % 16)), 1}));
    }
    state.ResumeTiming();
    for (const auto& o : orders)
      benchmark::DoNotOptimize(g.on_order(o));
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_RestingOrders)->Arg(10000)->Unit(benchmark::kMicrosecond);

static void BM_AggressiveMatch(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    state.PauseTiming();
    gw::MatchingGateway g;
    seed_asks(g, n);
    std::vector<std::vector<std::byte>> orders;
    orders.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      orders.push_back(gw::encode_new_order(
          20'000 + i, 1,
          gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc, 100, 1}));
    }
    state.ResumeTiming();
    for (const auto& o : orders)
      benchmark::DoNotOptimize(g.on_order(o));
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_AggressiveMatch)->Arg(10000)->Unit(benchmark::kMicrosecond);

static void BM_IngressToAckLatency(benchmark::State& state) {
  constexpr std::size_t kSamples = 8192;
  std::vector<double> samples;
  samples.reserve(kSamples);

  for (auto _ : state) {
    gw::MatchingGateway g;
    seed_asks(g, 64);
    samples.clear();
    for (std::size_t i = 0; i < kSamples; ++i) {
      auto pkt = gw::encode_new_order(
          30'000 + i, 1,
          gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc,
                             static_cast<int32_t>(80 - (i % 8)), 1});
      const auto t0 = std::chrono::steady_clock::now();
      auto reports = g.on_order(pkt);
      const auto t1 = std::chrono::steady_clock::now();
      samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
      benchmark::DoNotOptimize(reports);
    }
  }

  if (!samples.empty()) {
    std::sort(samples.begin(), samples.end());
    state.counters["median_ns"] = samples[samples.size() / 2];
    state.counters["p99_ns"] = samples[static_cast<std::size_t>(samples.size() * 0.99)];
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kSamples));
}
BENCHMARK(BM_IngressToAckLatency)->Unit(benchmark::kNanosecond)->Iterations(5);

BENCHMARK_MAIN();
