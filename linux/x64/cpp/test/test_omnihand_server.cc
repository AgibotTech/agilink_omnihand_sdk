// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "omnihand_client/rest_client.h"
#include "omnihand_client/websocket_client.h"

namespace {

std::string StartInstructions() {
  return
      "Start either the C++ or Python OmniHand Server before running this test:\n"
      "  C++:    omnihand_server\n"
      "  Python: python -m omnihand_server\n";
}

}  // namespace

TEST(OmniHandRestClientBlackBoxTest, PublicServerContract) {
  const std::string instructions = StartInstructions();
  std::cout << instructions;

  agilink::omnihand::client::OmniHandClientOptions options;
  options.timeout_ms = 3000;
  agilink::omnihand::client::OmniHandRestClient client(options);

  agilink::omnihand::client::Value health;
  try {
    health = client.Health();
  } catch (const std::exception& error) {
    FAIL() << "Cannot connect to OmniHand Server: " << error.what() << "\n"
           << instructions;
  }
  ASSERT_TRUE(health.IsObject());
  EXPECT_EQ(health.At("status").AsString(), "ok");

  const auto hands = client.ListHands();
  ASSERT_TRUE(hands.IsArray());

  const auto product_types = client.ListProductTypes();
  ASSERT_TRUE(product_types.IsArray());
  EXPECT_FALSE(product_types.Empty());

  const auto connection_types = client.ListConnectionTypes();
  ASSERT_TRUE(connection_types.IsArray());
  EXPECT_FALSE(connection_types.Empty());

  const auto methods = client.MethodsForProduct("omnihand_2025");
  ASSERT_TRUE(methods.IsObject());
  ASSERT_TRUE(methods.At("methods").IsArray());
  EXPECT_FALSE(methods.At("methods").Empty());

  try {
    client.CreateHand("not_a_product", "hcan");
    FAIL() << "invalid product type should be rejected";
  } catch (const agilink::omnihand::client::OmniHandClientError& error) {
    EXPECT_EQ(error.status_code(), 422);
    EXPECT_STREQ(error.what(), "Invalid hand_type: not_a_product");
    EXPECT_EQ(error.response().At("detail").AsString(), error.what());
  }
}

TEST(OmniHandWebSocketClientBlackBoxTest, PublicServerContract) {
  const std::string instructions = StartInstructions();
  std::cout << instructions;

  agilink::omnihand::client::OmniHandClientOptions options;
  options.timeout_ms = 3000;
  agilink::omnihand::client::OmniHandWebSocketClient client(options);

  try {
    client.Connect();
  } catch (const std::exception& error) {
    FAIL() << "Cannot connect to OmniHand Server WebSocket: " << error.what()
           << "\n" << instructions;
  }
  EXPECT_TRUE(client.IsConnected());

  const auto hands = client.ListHands();
  ASSERT_TRUE(hands.IsArray());

  const auto methods = client.MethodsForProduct("omnihand_2025");
  ASSERT_TRUE(methods.IsObject());
  ASSERT_TRUE(methods.At("methods").IsArray());
  EXPECT_FALSE(methods.At("methods").Empty());

  // Subscriptions are connection state, so their acknowledgement does not
  // require a physical hand. Stream production is exercised below when the
  // externally started Server already owns a hand session.
  constexpr int kUnusedHandId = 2147483647;
  const auto subscribed = client.Subscribe(kUnusedHandId, {"state"});
  ASSERT_TRUE(subscribed.IsObject());
  EXPECT_EQ(subscribed.At("hand_id").AsInt(), kUnusedHandId);
  ASSERT_EQ(subscribed.At("streams").Size(), 1U);
  EXPECT_EQ(subscribed.At("streams").AsArray().front().AsString(), "state");

  const auto unsubscribed = client.Unsubscribe(kUnusedHandId, {"state"});
  ASSERT_TRUE(unsubscribed.IsObject());
  EXPECT_EQ(unsubscribed.At("hand_id").AsInt(), kUnusedHandId);

  try {
    client.Request("not_a_message_type");
    FAIL() << "unsupported WebSocket request type should be rejected";
  } catch (const agilink::omnihand::client::OmniHandClientError& error) {
    EXPECT_EQ(error.status_code(), 0);
    EXPECT_STREQ(error.what(),
                 "Unsupported request type: not_a_message_type");
    EXPECT_EQ(error.response().At("type").AsString(), "result");
    EXPECT_FALSE(error.response().At("ok").AsBool());
  }

  client.Close();
  EXPECT_FALSE(client.IsConnected());
}

TEST(OmniHandWebSocketClientBlackBoxTest, ReceivesSubscribedEventWhenHandExists) {
  const std::string instructions = StartInstructions();
  std::cout << instructions;

  agilink::omnihand::client::OmniHandClientOptions options;
  options.timeout_ms = 3000;
  agilink::omnihand::client::OmniHandWebSocketClient client(options);
  agilink::omnihand::client::Value hands;
  try {
    hands = client.ListHands();
  } catch (const std::exception& error) {
    FAIL() << "Cannot connect to OmniHand Server WebSocket: " << error.what()
           << "\n" << instructions;
  }
  ASSERT_TRUE(hands.IsArray());
  if (hands.Empty()) {
    GTEST_SKIP() << "No Server hand session exists; command and subscription "
                    "acknowledgements are covered by PublicServerContract";
  }

  const auto& first_hand = hands.AsArray().front();
  ASSERT_TRUE(first_hand.IsObject());
  const int hand_id = static_cast<int>(first_hand.At("hand_id").AsInt());
  client.Subscribe(hand_id, {"state"});
  const auto event = client.NextEvent(3000);
  ASSERT_TRUE(event.IsObject());
  EXPECT_EQ(event.At("type").AsString(), "event");
  EXPECT_EQ(event.At("stream").AsString(), "state");
  EXPECT_EQ(event.At("hand_id").AsInt(), hand_id);
  EXPECT_TRUE(event.Contains("data"));
  client.Unsubscribe(hand_id, {"state"});
}
