// Concord
//
// Copyright (c) 2020-2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "client/thin-replica-client/thin_replica_client.hpp"
#include "client/thin-replica-client/grpc_connection.hpp"
#include "client/concordclient/trc_queue.hpp"

#include "gtest/gtest.h"
#include "thin_replica_client_mocks.hpp"

using com::vmware::concord::thin_replica::Data;
using com::vmware::concord::thin_replica::KVPair;
using std::condition_variable;
using std::make_shared;
using std::make_unique;
using std::mutex;
using std::atomic_bool;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::unique_ptr;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using concord::client::concordclient::EventVariant;
using concord::client::concordclient::Update;
using concord::client::concordclient::TrcQueue;
using client::thin_replica_client::ThinReplicaClient;
using client::thin_replica_client::ThinReplicaClientConfig;

const string kTestingClientID = "mock_client_id";
const string kTestingJaegerAddress = "127.0.0.1:6831";
const milliseconds kBriefDelayDuration = 10ms;

namespace {

TEST(thin_replica_client_test, test_destructor_always_successful) {
  Data update;
  update.mutable_events()->set_block_id(0);
  KVPair* events_data = update.mutable_events()->add_data();
  events_data->set_key("key");
  events_data->set_value("value");

  shared_ptr<MockDataStreamPreparer> stream_preparer(new RepeatedMockDataStreamPreparer(update));
  MockOrderedDataStreamHasher hasher(stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;
  unique_ptr<ThinReplicaClient> trc;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  EXPECT_NO_THROW(trc.reset()) << "ThinReplicaClient destructor failed.";
  update_queue->clear();

  for (const auto& ms : mock_servers) {
    ms->disconnect();
  }
  mock_servers.clear();

  mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  trc->Subscribe();
  update_queue->pop();
  update_queue->pop();
  trc_config.reset();
  EXPECT_NO_THROW(trc.reset()) << "ThinReplicaClient destructor failed when destructing a "
                                  "ThinReplicaClient with an active subscription.";
  update_queue->clear();
  for (const auto& ms : mock_servers) {
    ms->disconnect();
  }
  mock_servers.clear();

  mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  trc->Subscribe();
  update_queue->pop();
  update_queue->pop();
  trc->Unsubscribe();
  trc_config.reset();
  EXPECT_NO_THROW(trc.reset()) << "ThinReplicaClient destructor failed when destructing a "
                                  "ThinReplicaClient after ending its subscription.";
}

TEST(thin_replica_client_test, test_no_parameter_subscribe_success_cases) {
  Data update;
  update.mutable_events()->set_block_id(0);
  KVPair* events_data = update.mutable_events()->add_data();
  events_data->set_key("key");
  events_data->set_value("value");

  shared_ptr<MockDataStreamPreparer> stream_preparer(new RepeatedMockDataStreamPreparer(update));
  MockOrderedDataStreamHasher hasher(stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  auto trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  EXPECT_NO_THROW(trc->Subscribe()) << "ThinReplicaClient::Subscribe's no-parameter overload failed.";

  trc->Unsubscribe();
  EXPECT_NO_THROW(trc->Subscribe()) << "ThinReplicaClient::Subscribe's no-parameter overload failed when "
                                       "subscribing after closing a subscription.";
  EXPECT_NO_THROW(trc->Subscribe()) << "ThinReplicaClient::Subscribe's no-parameter overload failed when "
                                       "subscribing while there is an ongoing subscription.";
}

TEST(thin_replica_client_test, test_1_parameter_subscribe_success_cases) {
  Data update;
  update.mutable_events()->set_block_id(0);
  KVPair* events_data = update.mutable_events()->add_data();
  events_data->set_key("key");
  events_data->set_value("value");

  shared_ptr<MockDataStreamPreparer> stream_preparer(new RepeatedMockDataStreamPreparer(update));
  MockOrderedDataStreamHasher hasher(stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  auto trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  trc->Subscribe();
  auto update_received = update_queue->pop();
  EXPECT_TRUE(std::holds_alternative<Update>(*update_received));
  uint64_t block_id = std::get<Update>(*update_received).block_id;
  trc->Unsubscribe();
  EXPECT_NO_THROW(trc->Subscribe(block_id)) << "ThinReplicaClient::Subscribe's 1-parameter overload failed.";
  trc->Unsubscribe();

  trc->Unsubscribe();
  EXPECT_NO_THROW(trc->Subscribe(block_id)) << "ThinReplicaClient::Subscribe's 1-parameter overload failed after "
                                               "closing an existing subscription.";
  EXPECT_NO_THROW(trc->Subscribe(block_id)) << "ThinReplicaClient::Subscribe's 1-parameter overload failed when "
                                               "there is already an existing subscription.";

  for (size_t i = 0; i < 8; ++i) {
    trc->Unsubscribe();
    update_queue->clear();
    uint64_t previous_block_id = block_id;
    EXPECT_NO_THROW(trc->Subscribe(block_id + 1)) << "ThinReplicaClient::Subscribe's 1-parameter overload failed when "
                                                     "subscribing with a Block ID from a previously received block.";
    update_received = update_queue->pop();
    EXPECT_TRUE(std::holds_alternative<Update>(*update_received));
    block_id = std::get<Update>(*update_received).block_id;
    EXPECT_GT(block_id, previous_block_id) << "ThinReplicaClient::Subscribe's 1-parameter overload appears to be "
                                              "repeating already received blocks even when specifying where to "
                                              "start the subscription to avoid them.";
  }
}

TEST(thin_replica_client_test, test_1_parameter_subscribe_to_unresponsive_servers_fails) {
  Data update;
  update.mutable_events()->set_block_id(0);
  KVPair* events_data = update.mutable_events()->add_data();
  events_data->set_key("key");
  events_data->set_value("value");

  shared_ptr<MockDataStreamPreparer> stream_preparer(new RepeatedMockDataStreamPreparer(update));
  MockOrderedDataStreamHasher hasher(stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;
  size_t num_unresponsive = num_replicas - max_faulty;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher, num_unresponsive);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  auto trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  EXPECT_ANY_THROW(trc->Subscribe()) << "ThinReplicaClient::Subscribe's no-parameter overload doesn't throw an "
                                        "exception when trying to subscribe to a cluster with only max_faulty "
                                        "servers responsive.";
  trc_config.reset();
  trc.reset();
  for (const auto& ms : mock_servers) {
    ms->disconnect();
  }
  mock_servers.clear();

  mock_servers = CreateTrsConnections(num_replicas, num_replicas);
  trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  EXPECT_ANY_THROW(trc->Subscribe()) << "ThinReplicaClient::Subscribe's no-parameter overload doesn't throw an "
                                        "exception when trying to subscribe to a cluster with no responsive "
                                        "servers.";
}

TEST(thin_replica_client_test, test_unsubscribe_successful) {
  Data update;
  update.mutable_events()->set_block_id(0);
  KVPair* events_data = update.mutable_events()->add_data();
  events_data->set_key("key");
  events_data->set_value("value");

  shared_ptr<MockDataStreamPreparer> stream_preparer(new RepeatedMockDataStreamPreparer(update));
  MockOrderedDataStreamHasher hasher(stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  auto trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  EXPECT_NO_THROW(trc->Unsubscribe()) << "ThinReplicaClient::Unsubscribe failed for a newly-constructed "
                                         "ThinReplicaClient.";
  trc->Subscribe();
  update_queue->pop();
  update_queue->pop();
  EXPECT_NO_THROW(trc->Unsubscribe()) << "ThinReplicaClient::Unsubscribe failed for a ThinReplicaClient with "
                                         "an active subscription.";
  EXPECT_NO_THROW(trc->Unsubscribe()) << "ThinReplicaClient::Unsubscribe failed for a ThinReplicaClient with a "
                                         "subscription that had already been cancelled.";
}

TEST(thin_replica_client_test, test_pop_fetches_updates_) {
  Data update;
  update.mutable_events()->set_block_id(0);
  KVPair* events_data = update.mutable_events()->add_data();
  events_data->set_key("key");
  events_data->set_value("value");

  shared_ptr<MockDataStreamPreparer> base_stream_preparer(new RepeatedMockDataStreamPreparer(update, 1));
  auto delay_condition = make_shared<condition_variable>();
  auto spurious_wakeup_indicator = make_shared<atomic_bool>(true);
  auto delay_condition_mutex = make_shared<mutex>();
  shared_ptr<MockDataStreamPreparer> stream_preparer(new DelayedMockDataStreamPreparer(
      base_stream_preparer, delay_condition, spurious_wakeup_indicator, delay_condition_mutex));
  MockOrderedDataStreamHasher hasher(base_stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  auto trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  trc->Subscribe();
  auto update_received = update_queue->pop();
  EXPECT_TRUE((bool)update_received) << "ThinReplicaClient failed to publish update from initial state.";

  thread delay_thread([&]() {
    sleep_for(kBriefDelayDuration);
    *spurious_wakeup_indicator = false;
    delay_condition->notify_one();
  });
  update_received = update_queue->pop();
  EXPECT_TRUE((bool)update_received) << "ThinReplicaClient failed to publish update received from servers "
                                        "while the application is already waiting on the update queue.";
  delay_thread.join();

  // The current implementation of the ThinReplicaClient may block on Read calls
  // trying to join threads before it completes its destructor, so we unblock
  // any such calls here.
  *spurious_wakeup_indicator = false;
  sleep_for(kBriefDelayDuration);
  delay_condition->notify_all();
}

TEST(thin_replica_client_test, test_acknowledge_block_id_success) {
  Data update;
  update.mutable_events()->set_block_id(0);
  KVPair* events_data = update.mutable_events()->add_data();
  events_data->set_key("key");
  events_data->set_value("value");

  shared_ptr<MockDataStreamPreparer> stream_preparer(new RepeatedMockDataStreamPreparer(update));
  MockOrderedDataStreamHasher hasher(stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  auto trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  EXPECT_NO_THROW(trc->AcknowledgeBlockID(1)) << "ThinReplicaClient::AcknowledgeBlockID fails when called on a "
                                                 "freshly-constructed ThinReplicaClient.";
  trc->Subscribe();
  update_queue->pop();
  update_queue->pop();
  EXPECT_NO_THROW(trc->AcknowledgeBlockID(2)) << "ThinReplicaClient::AcknowledgeBlockID fails when called on a "
                                                 "ThinReplicaClient with an active subscription.";
  trc->Unsubscribe();
  EXPECT_NO_THROW(trc->AcknowledgeBlockID(3)) << "ThinReplicaClient::AcknowledgeBlockID fails when called on a "
                                                 "ThinReplicaClient with an ended subscription.";
  for (uint64_t block_id = 0; block_id <= UINT32_MAX; block_id += (block_id + 1)) {
    EXPECT_NO_THROW(trc->AcknowledgeBlockID(block_id))
        << "ThinReplicaClient::AcknowledgeBlockID fails when called with "
           "arbitrary block IDs.";
  }
}

TEST(thin_replica_client_test, test_correct_data_returned_) {
  vector<Data> update_data;
  for (size_t i = 0; i <= 60; ++i) {
    Data update;
    update.mutable_events()->set_block_id(i);
    for (size_t j = 1; j <= 12; ++j) {
      if (i % j == 0) {
        KVPair* kvp = update.mutable_events()->add_data();
        kvp->set_key("key" + to_string(j));
        kvp->set_value("value" + to_string(i / j));
      }
    }
    update_data.push_back(update);
  }
  size_t num_initial_updates = 6;

  shared_ptr<MockDataStreamPreparer> base_stream_preparer(
      new VectorMockDataStreamPreparer(update_data, num_initial_updates));
  auto delay_condition = make_shared<condition_variable>();
  auto spurious_wakeup_indicator = make_shared<atomic_bool>(true);
  auto delay_condition_mutex = make_shared<mutex>();
  shared_ptr<MockDataStreamPreparer> stream_preparer(new DelayedMockDataStreamPreparer(
      base_stream_preparer, delay_condition, spurious_wakeup_indicator, delay_condition_mutex));
  MockOrderedDataStreamHasher hasher(base_stream_preparer);

  uint16_t max_faulty = 1;
  size_t num_replicas = 3 * max_faulty + 1;

  shared_ptr<TrcQueue> update_queue = make_shared<TrcQueue>();

  auto mock_servers = CreateTrsConnections(num_replicas, stream_preparer, hasher);
  auto trc_config = make_unique<ThinReplicaClientConfig>(kTestingClientID, update_queue, max_faulty, mock_servers);
  std::shared_ptr<concordMetrics::Aggregator> aggregator;
  auto trc = make_unique<ThinReplicaClient>(std::move(trc_config), aggregator);
  EXPECT_FALSE((bool)(update_queue->tryPop())) << "ThinReplicaClient appears to have published state to update queue "
                                                  "prior to subscription.";
  trc->Subscribe();
  trc->Unsubscribe();
  for (size_t i = 0; i < num_initial_updates; ++i) {
    unique_ptr<EventVariant> received_update = update_queue->tryPop();
    Data& expected_update = update_data[i];
    EXPECT_TRUE((bool)received_update) << "ThinReplicaClient failed to fetch an expected update included in "
                                          "the initial state.";
    EXPECT_TRUE(std::holds_alternative<Update>(*received_update));
    auto& legacy_event = std::get<Update>(*received_update);
    EXPECT_EQ(legacy_event.block_id, expected_update.events().block_id())
        << "An update the ThinReplicaClient fetched in the initial state has "
           "an incorrect block ID.";
    EXPECT_EQ(legacy_event.kv_pairs.size(), expected_update.events().data_size())
        << "An update the ThinReplicaClient fetched in the initial state has "
           "an incorrect number of KV-pair updates.";
    for (size_t j = 0; j < legacy_event.kv_pairs.size() && j < (size_t)expected_update.events().data_size(); ++j) {
      EXPECT_EQ(legacy_event.kv_pairs[j].first, expected_update.events().data(j).key())
          << "A key in an update the ThinReplicaClient fetched in the initial "
             "state does not match its expected value.";
      EXPECT_EQ(legacy_event.kv_pairs[j].second, expected_update.events().data(j).value())
          << "A value in an update the ThinReplicaClient fetched in the "
             "initial state does not match its expected value.";
    }
  }
  EXPECT_FALSE((bool)(update_queue->tryPop())) << "ThinReplicaClient appears to have collected an unexpected number of "
                                                  "updates in its initial state.";
  *spurious_wakeup_indicator = false;
  delay_condition->notify_all();
  sleep_for(kBriefDelayDuration);
  EXPECT_FALSE((bool)(update_queue->tryPop())) << "ThinReplicaClient appears to have received an update after "
                                                  "unsubscribing.";

  trc->Subscribe(num_initial_updates);
  for (size_t i = num_initial_updates; i < update_data.size(); ++i) {
    unique_ptr<EventVariant> received_update = update_queue->pop();
    Data& expected_update = update_data[i];

    EXPECT_TRUE((bool)received_update) << "ThinReplicaClient failed to fetch an expected update from an "
                                          "ongoing subscription.";
    EXPECT_TRUE(std::holds_alternative<Update>(*received_update));
    auto& legacy_event = std::get<Update>(*received_update);
    EXPECT_EQ(legacy_event.block_id, expected_update.events().block_id())
        << "An update the ThinReplicaClient received from an ongoing "
           "subscription has an incorrect Block ID.";
    EXPECT_EQ(legacy_event.kv_pairs.size(), expected_update.events().data_size())
        << "An update the ThinReplicaClient received in an ongoing "
           "subscription has an incorrect number of KV-pair updates.";
    for (size_t j = 0; j < legacy_event.kv_pairs.size() && j < (size_t)expected_update.events().data_size(); ++j) {
      EXPECT_EQ(legacy_event.kv_pairs[j].first, expected_update.events().data(j).key())
          << "A key in an update the ThinReplicaClient received in an ongoing "
             "subscription does not match its expected value.";
      EXPECT_EQ(legacy_event.kv_pairs[j].second, expected_update.events().data(j).value())
          << "A value in an update the ThinReplicaClient received in an "
             "ongoing subscription does not match its expected value.";
    }
  }

  // The current implementation of the ThinReplicaClient may block on Read calls
  // trying to join threads before it completes its destructor, so we unblock
  // any such calls here.
  *spurious_wakeup_indicator = false;
  sleep_for(kBriefDelayDuration);
  delay_condition->notify_all();
}

}  // anonymous namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
