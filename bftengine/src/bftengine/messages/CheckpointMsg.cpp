// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").  You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "CheckpointMsg.hpp"
#include "assertUtils.hpp"
#include "SigManager.hpp"
#include "EpochManager.hpp"

namespace bftEngine {
namespace impl {

char* default_rvb_digest(unsigned int rvb_data) {
  char rvb_data_digest[DIGEST_SIZE] = {0};
  concord::util::digest::DigestUtil::Context c;
  c.update(reinterpret_cast<const char*>(rvb_data), sizeof(rvb_data));
  c.writeDigest(rvb_data_digest);
  return std::move(rvb_data_digest);
}

const char* initial_rvb_data_digest = default_rvb_digest(INITIAL_RVB_DATA);
const char* no_rvb_data_digest = default_rvb_digest(NO_RVB_DATA);

CheckpointMsg::CheckpointMsg(ReplicaId genReplica,
                             SeqNum seqNum,
                             std::uint64_t state,
                             const Digest& stateDigest,
                             const Digest& otherDigest,
                             const Digest& rvbDataDigest,
                             bool stateIsStable,
                             const concordUtils::SpanContext& spanContext)
    : MessageBase(genReplica,
                  MsgCode::Checkpoint,
                  spanContext.data().size(),
                  sizeof(Header) + SigManager::instance()->getMySigLength()) {
  b()->seqNum = seqNum;
  b()->epochNum = EpochManager::instance().getSelfEpochNumber();
  b()->state = state;
  b()->stateDigest = stateDigest;
  b()->otherDigest = otherDigest;
  b()->rvbDataDigest = rvbDataDigest;
  b()->flags = 0;
  b()->genReplicaId = genReplica;
  if (stateIsStable) b()->flags |= 0x1;
  std::memcpy(body() + sizeof(Header), spanContext.data().data(), spanContext.data().size());
}

void CheckpointMsg::sign() {
  auto sigManager = SigManager::instance();
  sigManager->sign(body(), sizeof(Header), body() + sizeof(Header) + spanContextSize(), sigManager->getMySigLength());
}

void CheckpointMsg::validate(const ReplicasInfo& repInfo) const {
  ConcordAssert(type() == MsgCode::Checkpoint);
  ConcordAssert(senderId() != repInfo.myId());

  auto sigManager = SigManager::instance();

  if (size() < sizeof(Header) + spanContextSize() || !repInfo.isIdOfReplica(senderId()) ||
      !repInfo.isIdOfReplica(idOfGeneratedReplica()) || (seqNumber() % checkpointWindowSize != 0) ||
      (digestOfState().isZero() && otherDigest().isZero() && rvbDataDigest().isZero())) {
    throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": basic validations"));
  }

  auto sigLen = sigManager->getSigLength(idOfGeneratedReplica());

  if (size() < sizeof(Header) + spanContextSize() + sigLen) {
    throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": size"));
  }

  if (!sigManager->verifySig(
          idOfGeneratedReplica(), body(), sizeof(Header), body() + sizeof(Header) + spanContextSize(), sigLen))
    throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": verifySig"));
  // TODO(GG): consider to protect against messages that are larger than needed (here and in other messages)
}

}  // namespace impl
}  // namespace bftEngine
