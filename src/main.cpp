#include "gw/gateway.hpp"
#include "gw/order_protocol.hpp"
#include "gw/session.hpp"

#include "mdf/protocol.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage(const char* argv0) {
  std::cerr << "usage:\n"
            << "  " << argv0 << " smoke\n"
            << "  " << argv0 << " replay-demo\n";
}

int cmd_smoke() {
  gw::MatchingGateway gw;
  gw::SessionRecorder rec;

  auto md1 = mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 5});
  auto md2 = mdf::encode_add(2, 1, mdf::AddMsg{11, mdf::Side::Ask, 100, 5});
  rec.write(gw::StreamChannel::MarketData, md1);
  rec.write(gw::StreamChannel::MarketData, md2);
  gw.on_market_data(md1);
  gw.on_market_data(md2);

  auto ord = gw::encode_new_order(
      1001, 1, gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc, 100, 7});
  rec.write(gw::StreamChannel::Order, ord);
  auto reports = gw.on_order(ord);

  std::cout << "book_size=" << gw.book().Size() << " reports=" << reports.size()
            << " trades=" << gw.stats().trades << " session_bytes=" << rec.data().size() << "\n";
  for (const auto& r : reports) {
    std::cout << " exec type=" << static_cast<int>(r.type) << " id=" << r.client_order_id
              << " qty=" << r.qty << " px=" << r.price << "\n";
  }
  return 0;
}

int cmd_replay_demo() {
  gw::MatchingGateway live;
  gw::SessionRecorder rec;

  std::vector<std::vector<std::byte>> actions;
  actions.push_back(mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 5}));
  actions.push_back(mdf::encode_add(2, 1, mdf::AddMsg{11, mdf::Side::Ask, 100, 5}));
  actions.push_back(gw::encode_new_order(
      1001, 1, gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc, 100, 8}));

  for (std::size_t i = 0; i < actions.size(); ++i) {
    if (i < 2) {
      rec.write(gw::StreamChannel::MarketData, actions[i]);
      live.on_market_data(actions[i]);
    } else {
      rec.write(gw::StreamChannel::Order, actions[i]);
      live.on_order(actions[i]);
    }
  }

  gw::MatchingGateway replayed;
  for (const auto& pkt : gw::parse_session(rec.data())) {
    if (pkt.channel == gw::StreamChannel::MarketData)
      replayed.on_market_data(pkt.bytes);
    else
      replayed.on_order(pkt.bytes);
  }

  const bool ok = live.trade_log().size() == replayed.trade_log().size() &&
                  live.book().Size() == replayed.book().Size() &&
                  live.stats().trades == replayed.stats().trades;
  std::cout << "replay_ok=" << (ok ? 1 : 0) << " trades=" << live.stats().trades
            << " book=" << live.book().Size() << "\n";
  return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }
  const std::string cmd = argv[1];
  if (cmd == "smoke")
    return cmd_smoke();
  if (cmd == "replay-demo")
    return cmd_replay_demo();
  print_usage(argv[0]);
  return 1;
}
