// Concord
//
// Copyright (c) 2018-2022 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <memory>
#include <chrono>
#include <thread>

#include "Logger.hpp"
#include "OpenTracing.hpp"
#include "sliver.hpp"
#include "db_interfaces.h"
#include "block_metadata.hpp"
#include "KVBCInterfaces.h"
#include "ControlStateManager.hpp"
#include "kv_replica_msgs.cmf.hpp"
#include "SharedTypes.hpp"
#include "categorization/db_categories.h"
#include "kvbc_adapter/replica_adapter.hpp"

static const std::string VERSIONED_KV_CAT_ID{concord::kvbc::categorization::kExecutionPrivateCategory};
static const std::string BLOCK_MERKLE_CAT_ID{concord::kvbc::categorization::kExecutionProvableCategory};

class KVCommandHandler : public concord::kvbc::ICommandsHandler {
 public:
  KVCommandHandler(concord::kvbc::IReader *storage,
                   concord::kvbc::IBlockAdder *blocksAdder,
                   concord::kvbc::IBlockMetadata *blockMetadata,
                   bool addAllKeysAsPublic = false,
                   concord::kvbc::adapter::ReplicaBlockchain *kvbc = nullptr)
      : storageReader_(storage),
        blockAdder_(blocksAdder),
        blockMetadata_(blockMetadata),
        addAllKeysAsPublic_{addAllKeysAsPublic},
        kvbc_{kvbc} {
    if (addAllKeysAsPublic_) {
      ConcordAssertNE(kvbc_, nullptr);
    }
  }

  void execute(ExecutionRequestsQueue &requests,
               std::optional<bftEngine::Timestamp> timestamp,
               const std::string &batchCid,
               concordUtils::SpanWrapper &parentSpan) override;

  void preExecute(IRequestsHandler::ExecutionRequest &req,
                  std::optional<bftEngine::Timestamp> timestamp,
                  const std::string &batchCid,
                  concordUtils::SpanWrapper &parentSpan) override;

  void setPerformanceManager(std::shared_ptr<concord::performance::PerformanceManager> perfManager) override;

 private:
  void add(std::string &&key,
           std::string &&value,
           concord::kvbc::categorization::VersionedUpdates &,
           concord::kvbc::categorization::BlockMerkleUpdates &) const;

  bftEngine::OperationResult executeWriteCommand(
      uint32_t requestSize,
      const char *request,
      uint64_t sequenceNum,
      uint8_t flags,
      size_t maxReplySize,
      char *outReply,
      uint32_t &outReplySize,
      bool isBlockAccumulationEnabled,
      concord::kvbc::categorization::VersionedUpdates &blockAccumulatedVerUpdates,
      concord::kvbc::categorization::BlockMerkleUpdates &blockAccumulatedMerkleUpdates);

  bftEngine::OperationResult executeReadOnlyCommand(uint32_t requestSize,
                                                    const char *request,
                                                    size_t maxReplySize,
                                                    char *outReply,
                                                    uint32_t &outReplySize,
                                                    uint32_t &specificReplicaInfoOutReplySize);

  bftEngine::OperationResult verifyWriteCommand(uint32_t requestSize,
                                                const uint8_t *request,
                                                size_t maxReplySize,
                                                uint32_t &outReplySize) const;

  bftEngine::OperationResult executeReadCommand(const concord::osexample::kv::messages::KVReadRequest &request,
                                                size_t maxReplySize,
                                                char *outReply,
                                                uint32_t &outReplySize);

  bftEngine::OperationResult executeGetBlockDataCommand(
      const concord::osexample::kv::messages::KVGetBlockDataRequest &request,
      size_t maxReplySize,
      char *outReply,
      uint32_t &outReplySize);

  bftEngine::OperationResult executeGetLastBlockCommand(size_t maxReplySize, char *outReply, uint32_t &outReplySize);

  void addMetadataKeyValue(concord::kvbc::categorization::VersionedUpdates &updates, uint64_t sequenceNum) const;

 private:
  std::optional<std::string> get(const std::string &key, concord::kvbc::BlockId blockId) const;
  std::string getAtMost(const std::string &key, concord::kvbc::BlockId blockId) const;
  std::string getLatest(const std::string &key) const;
  std::optional<concord::kvbc::BlockId> getLatestVersion(const std::string &key) const;
  std::optional<std::map<std::string, std::string>> getBlockUpdates(concord::kvbc::BlockId blockId) const;
  void writeAccumulatedBlock(ExecutionRequestsQueue &blockedRequests,
                             concord::kvbc::categorization::VersionedUpdates &verUpdates,
                             concord::kvbc::categorization::BlockMerkleUpdates &merkleUpdates,
                             uint64_t sn);
  void addBlock(concord::kvbc::categorization::VersionedUpdates &verUpdates,
                concord::kvbc::categorization::BlockMerkleUpdates &merkleUpdates,
                uint64_t sn);
  void addKeys(const concord::osexample::kv::messages::KVWriteRequest &writeReq,
               uint64_t sequenceNum,
               concord::kvbc::categorization::VersionedUpdates &verUpdates,
               concord::kvbc::categorization::BlockMerkleUpdates &merkleUpdates);
  bool hasConflictInBlockAccumulatedRequests(
      const std::string &key,
      concord::kvbc::categorization::VersionedUpdates &blockAccumulatedVerUpdates,
      concord::kvbc::categorization::BlockMerkleUpdates &blockAccumulatedMerkleUpdates) const;

  logging::Logger getLogger() const {
    static logging::Logger logger_(logging::getLogger("osexample::KVCommandHandler"));
    return logger_;
  }

 private:
  concord::kvbc::IReader *storageReader_;
  concord::kvbc::IBlockAdder *blockAdder_;
  concord::kvbc::IBlockMetadata *blockMetadata_;
  size_t readsCounter_ = 0;
  size_t writesCounter_ = 0;
  size_t getLastBlockCounter_ = 0;
  std::shared_ptr<concord::performance::PerformanceManager> perfManager_;
  bool addAllKeysAsPublic_{false};  // Add all key-values in the block merkle category as public ones.
  concord::kvbc::adapter::ReplicaBlockchain *kvbc_{nullptr};
};
