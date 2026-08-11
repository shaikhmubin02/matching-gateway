#include "gw/gateway.hpp"
#include "gw/order_protocol.hpp"
#include "gw/session.hpp"

#include "mdf/protocol.hpp"

#include <gtest/gtest.h>

TEST(Gateway, MdSeedsThenAggressiveMatchesFifo) {
  gw::MatchingGateway g;
  g.on_market_data(mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 5}));
  g.on_market_data(mdf::encode_add(2, 1, mdf::AddMsg{11, mdf::Side::Ask, 100, 5}));
  EXPECT_EQ(g.book().Size(), 2u);

  auto reports = g.on_order(gw::encode_new_order(
      1001, 1, gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc, 100, 5}));

  ASSERT_GE(reports.size(), 2u);
  EXPECT_EQ(reports[0].type, gw::ExecType::Ack);
  EXPECT_EQ(reports[1].type, gw::ExecType::Trade);
  EXPECT_EQ(reports[1].other_order_id, 10u);
  EXPECT_EQ(g.book().Size(), 1u);
  EXPECT_EQ(g.stats().trades, 1u);
}

TEST(Gateway, FokMissRejectsAndLeavesBook) {
  gw::MatchingGateway g;
  g.on_market_data(mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 3}));
  const auto before = g.book().Size();

  auto reports = g.on_order(gw::encode_new_order(
      2001, 1, gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Fok, 100, 10}));

  ASSERT_EQ(reports.size(), 1u);
  EXPECT_EQ(reports[0].type, gw::ExecType::Reject);
  EXPECT_EQ(reports[0].reason, gw::RejectReason::FokUnfilled);
  EXPECT_EQ(g.book().Size(), before);
}

TEST(Gateway, DuplicateOrderIdRejected) {
  gw::MatchingGateway g;
  g.on_market_data(mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 110, 1}));

  auto a = g.on_order(gw::encode_new_order(
      3001, 1, gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc, 90, 1}));
  EXPECT_EQ(a[0].type, gw::ExecType::Ack);

  auto b = g.on_order(gw::encode_new_order(
      3001, 1, gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc, 90, 1}));
  ASSERT_EQ(b.size(), 1u);
  EXPECT_EQ(b[0].type, gw::ExecType::Reject);
  EXPECT_EQ(b[0].reason, gw::RejectReason::DuplicateId);
}

TEST(Gateway, DuplicateMarketDataDropped) {
  gw::MatchingGateway g;
  auto pkt = mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 1});
  g.on_market_data(pkt);
  const auto applied = g.stats().md_applied;
  const auto size = g.book().Size();
  g.on_market_data(pkt);
  EXPECT_EQ(g.feed().stats().duplicates, 1u);
  EXPECT_EQ(g.stats().md_applied, applied);
  EXPECT_EQ(g.book().Size(), size);
}

TEST(Gateway, OutOfOrderMarketDataBufferedThenFlushed) {
  gw::MatchingGateway g;
  g.on_market_data(mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 1}));
  g.on_market_data(mdf::encode_add(3, 1, mdf::AddMsg{12, mdf::Side::Ask, 101, 1}));
  EXPECT_EQ(g.book().Size(), 1u);
  g.on_market_data(mdf::encode_add(2, 1, mdf::AddMsg{11, mdf::Side::Ask, 102, 1}));
  EXPECT_EQ(g.book().Size(), 3u);
}

TEST(Gateway, GapBeyondWindowDetected) {
  gw::MatchingGateway g;
  g.on_market_data(mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 1}));
  g.on_market_data(mdf::encode_add(100, 1, mdf::AddMsg{12, mdf::Side::Ask, 101, 1}));
  EXPECT_GE(g.feed().stats().gaps, 1u);
}

TEST(Gateway, RecordReplayIdenticalTradeLog) {
  gw::SessionRecorder rec;
  gw::MatchingGateway live;

  auto md1 = mdf::encode_add(1, 1, mdf::AddMsg{10, mdf::Side::Ask, 100, 5});
  auto md2 = mdf::encode_add(2, 1, mdf::AddMsg{11, mdf::Side::Ask, 100, 5});
  auto ord = gw::encode_new_order(
      1001, 1, gw::ClientNewOrder{gw::Side::Buy, gw::TimeInForce::Gtc, 100, 8});

  rec.write(gw::StreamChannel::MarketData, md1);
  rec.write(gw::StreamChannel::MarketData, md2);
  rec.write(gw::StreamChannel::Order, ord);
  live.on_market_data(md1);
  live.on_market_data(md2);
  live.on_order(ord);

  gw::MatchingGateway replayed;
  for (const auto& pkt : gw::parse_session(rec.data())) {
    if (pkt.channel == gw::StreamChannel::MarketData)
      replayed.on_market_data(pkt.bytes);
    else
      replayed.on_order(pkt.bytes);
  }

  ASSERT_EQ(live.trade_log().size(), replayed.trade_log().size());
  for (std::size_t i = 0; i < live.trade_log().size(); ++i) {
    EXPECT_EQ(live.trade_log()[i].client_order_id, replayed.trade_log()[i].client_order_id);
    EXPECT_EQ(live.trade_log()[i].other_order_id, replayed.trade_log()[i].other_order_id);
    EXPECT_EQ(live.trade_log()[i].qty, replayed.trade_log()[i].qty);
    EXPECT_EQ(live.trade_log()[i].price, replayed.trade_log()[i].price);
  }
  EXPECT_EQ(live.book().Size(), replayed.book().Size());
}
