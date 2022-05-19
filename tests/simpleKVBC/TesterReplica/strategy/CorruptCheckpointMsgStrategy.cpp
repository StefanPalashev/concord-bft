// Concord
//
// Copyright (c) 2022 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#include "CorruptCheckpointMsgStrategy.hpp"
#include "messages/CheckpointMsg.hpp"
#include "SigManager.hpp"

namespace concord::kvbc::strategy {

std::string CorruptCheckpointMsgStrategy::getStrategyName() { return CLASSNAME(CorruptCheckpointMsgStrategy); }
uint16_t CorruptCheckpointMsgStrategy::getMessageCode() { return static_cast<uint16_t>(MsgCode::Checkpoint); }

bool CorruptCheckpointMsgStrategy::changeMessage(std::shared_ptr<bftEngine::impl::MessageBase>& msg) {
  CheckpointMsg& checkpoint_message = static_cast<CheckpointMsg&>(*(msg.get()));
  LOG_INFO(logger_, KVLOG(replicas_to_corrupt_.size(), checkpoint_message.senderId()));

  if (replicas_to_corrupt_.count(checkpoint_message.senderId())) {
    Digest& current_rvb_data_digest = checkpoint_message.rvbDataDigest();
    std::string rvb_data_digest_string = std::string(current_rvb_data_digest.getForUpdate());
    // Modify the 1st byte for now
    LOG_INFO(logger_, "Before changing:" << KVLOG(current_rvb_data_digest, rvb_data_digest_string));
    rvb_data_digest_string[0]++;
    current_rvb_data_digest = Digest(const_cast<char*>(rvb_data_digest_string.data()), rvb_data_digest_string.size());
    LOG_INFO(logger_, "After changing:" << KVLOG(current_rvb_data_digest, rvb_data_digest_string));
      
    auto sigManager = SigManager::instance();
    sigManager->sign(checkpoint_message.body(),
                     sizeOfHeader<CheckpointMsg>(),
                     checkpoint_message.body() + sizeOfHeader<CheckpointMsg>() + checkpoint_message.spanContextSize(),
                     sigManager->getMySigLength());
    return true;
  }
  return false;
}
}  // end of namespace concord::kvbc::strategy
