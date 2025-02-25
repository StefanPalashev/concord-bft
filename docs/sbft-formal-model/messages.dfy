// Concord
//
// Copyright (c) 2022 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").  You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the sub-component's license, as noted in the LICENSE
// file.

include "network.s.dfy"
include "library/Library.dfy"

module Messages {
  import Library
  import opened HostIdentifiers
  import Network

  type SequenceID = k:nat | 0 < k witness 1
  type ViewNum = nat

  datatype ClientOperation = ClientOperation(sender:HostId, timestamp:nat)

  datatype OperationWrapper = Noop | ClientOp(clientOperation: ClientOperation)

  type CommittedClientOperations = map<SequenceID, OperationWrapper>

  function sendersOf(msgs:set<Network.Message<Message>>) : set<HostIdentifiers.HostId> {
    set msg | msg in msgs :: msg.sender
  }

  datatype PreparedCertificate = PreparedCertificate(votes:set<Network.Message<Message>>) {
    function prototype() : Message 
      requires |votes| > 0
    {
      var prot :| prot in votes;
      prot.payload
    }
    predicate WF() {
      (forall v | v in votes :: v.payload.Prepare?)
    }
    predicate valid(quorumSize:nat) {
      || empty()
      || (&& |votes| == quorumSize
          && WF()
          && (forall v | v in votes :: v.payload == prototype()) // messages have to be votes that match eachother by the prototype 
          && UniqueSenders(votes))
    }
    predicate empty() {
      && |votes| == 0
    }
  }

  datatype ViewChangeMsgsSelectedByPrimary = ViewChangeMsgsSelectedByPrimary(msgs:set<Network.Message<Message>>) {
    predicate valid(view:ViewNum, quorumSize:nat) {
      && |msgs| > 0
      && (forall v | v in msgs :: && v.payload.ViewChangeMsg?
                                  && v.payload.validViewChangeMsg(quorumSize)
                                  && v.payload.newView == view) // All the ViewChange messages have to be for the same View. 
      && UniqueSenders(msgs)
      && |msgs| == quorumSize //TODO: once proof is complete try with >=
    }
  }

  datatype CheckpointsQuorum = CheckpointsQuorum(msgs:set<Network.Message<Message>>) {
    function prototype() : Message 
      requires |msgs| > 0
    {
      var prot :| prot in msgs;
      prot.payload
    }
    predicate valid(lastStableCheckpoint:SequenceID, quorumSize:nat) {
      && |msgs| > 0
      && UniqueSenders(msgs)
      && (forall m | m in msgs :: && m.payload.CheckpointMsg?
                                  && m.payload == prototype()
                                  && m.payload.seqIDReached == lastStableCheckpoint)
      && |msgs| >= quorumSize
    }
  }

  predicate UniqueSenders(msgs:set<Network.Message<Message>>) {
    (forall m1, m2 | && m1 in msgs
                     && m2 in msgs
                     && m1 != m2
                       :: m1.sender != m2.sender)
  }

  // Define your Message datatype here.
  datatype Message = | PrePrepare(view:ViewNum, seqID:SequenceID, operationWrapper:OperationWrapper)
                     | Prepare(view:ViewNum, seqID:SequenceID, operationWrapper:OperationWrapper)
                     | Commit(view:ViewNum, seqID:SequenceID, operationWrapper:OperationWrapper)
                     | ClientRequest(clientOp:ClientOperation)
                     | ViewChangeMsg(newView:ViewNum,
                                     lastStableCheckpoint:SequenceID,
                                     proofForLastStable:CheckpointsQuorum,
                                     certificates:map<SequenceID, PreparedCertificate>)
                     | NewViewMsg(newView:ViewNum, vcMsgs:ViewChangeMsgsSelectedByPrimary) 
                     | CheckpointMsg(seqIDReached:SequenceID, committedClientOperations:CommittedClientOperations)
                     {
                       predicate valid(quorumSize:nat)
                       {
                         && (ViewChangeMsg? ==> validViewChangeMsg(quorumSize))
                         && (NewViewMsg? ==> validNewViewMsg(quorumSize))
                       }
                       predicate validViewChangeMsg(quorumSize:nat) 
                         requires ViewChangeMsg?
                       {
                         proofForLastStable.valid(lastStableCheckpoint, quorumSize)
                       }
                       predicate validNewViewMsg(quorumSize:nat) 
                         requires NewViewMsg?
                       {
                         vcMsgs.valid(newView, quorumSize)
                       }
                       predicate IsIntraViewMsg() {
                          || PrePrepare?
                          || Prepare?
                          || Commit? 
                       }
                     }
  // ToDo: ClientReply
}
