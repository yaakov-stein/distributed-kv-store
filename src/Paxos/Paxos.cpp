// src/Paxos/Paxos.cpp

#include "Paxos.hpp"
#include "Emulation.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>

namespace Paxos {

using clock = std::chrono::steady_clock;

//--------------------------------------------------------------------------------
// String split + serialize/deserialize
//--------------------------------------------------------------------------------
static std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, sep)) out.push_back(tok);
    return out;
}

std::string serializeMessage(const Message &m) {
    std::ostringstream ss;
    ss << int(m.type) << '|'
       << m.slot << '|'
       << m.ballot.round << '|'
       << m.ballot.proposerId << '|'
       << int(m.command.operation) << '|'
       << m.command.key << '|'
       << m.command.value << '|'
       << m.senderId << '|'
       << m.candidateId;
    return ss.str();
}

Message deserializeMessage(const std::string &raw) {
    auto parts = split(raw, '|');
    Message m;
    m.type        = MessageType(std::stoi(parts[0]));
    m.slot        = std::stoi(parts[1]);
    m.ballot.round    = std::stoi(parts[2]);
    m.ballot.proposerId = std::stoi(parts[3]);
    m.command.operation = Command::OpType(std::stoi(parts[4]));
    m.command.key        = parts[5];
    m.command.value      = std::stoi(parts[6]);
    m.senderId           = std::stoi(parts[7]);
    m.candidateId        = std::stoi(parts[8]);
    return m;
}

//--------------------------------------------------------------------------------
// Emulation hooks
//--------------------------------------------------------------------------------
template<> void MultiPaxos::sendMessageTo<Message>(int tgt, const Message &m) {
    em->send(tgt, serializeMessage(m));
}
template<> void MultiPaxos::broadcastMessage<Message>(const Message &m) {
    em->broadcast(serializeMessage(m));
}

//--------------------------------------------------------------------------------
// Constructor / state
//--------------------------------------------------------------------------------
MultiPaxos::MultiPaxos(int id_, const Configuration &cfg_, Emu::Emulation* emulator)
  : id(id_), config(cfg_), em(emulator),
    leaderState(LeaderState::MEMBER),
    leaderId(-1),
    currentBallot{0, id_},
    electionBallot{0, id_},
    votedForBallot{0, -1},
    nextSlot(0),
    stop(false)
{
    lastHeartbeat.store(std::chrono::steady_clock::now());
    std::cout << "[P" << id << "] Starting\n";
}

void MultiPaxos::shutdown()        { stop.store(true); }
auto MultiPaxos::getState() const  -> std::unordered_map<std::string,int> {
    return keyValueStore;
}

//--------------------------------------------------------------------------------
// Main loop
//--------------------------------------------------------------------------------
void MultiPaxos::run() {
    // std::thread(&MultiPaxos::checkHeartbeat, this).detach();
    std::thread([this]() {
        Emu::Emulation::currentPid = this->id;
        this->checkHeartbeat();
        }).detach();
    while (!stop.load()) {
        auto [sender, raw] = em->receiveMessage();
        std::cout << "[P" << id << "] Received raw msg: " << raw << "\n";
        auto msg = deserializeMessage(raw);
        switch (msg.type) {
          case MessageType::CLIENT: {
            // int slot = (msg.slot >= 0 ? msg.slot : nextSlot);
            // proposeCommand(slot, msg.command);
            // nextSlot = std::max(nextSlot, slot+1);
            proposeCommand(nextSlot, msg.command);
            ++nextSlot;
            break;
          }
          case MessageType::ELECTION:
          case MessageType::VOTE:
          case MessageType::LEADER_ANNOUNCE:
            onElectionMessage(msg);
            break;
          case MessageType::HEARTBEAT:
            onHeartbeat(msg);
            break;
          default:
            onPaxosMessage(msg);
            break;
        }
    }
}

//------------------------------------------------------------------------------
// Leader election
//------------------------------------------------------------------------------

void MultiPaxos::becomeCandidate() {
    leaderState = LeaderState::CANDIDATE;
    electionBallot.round++;
    electionBallot.proposerId = id;
  
    // clear any old votes and promise
    electionVoteCount = 1;              // implicit “vote for myself”
    electionPromise = electionBallot;   // promise to myself

    Message m{ MessageType::ELECTION, 0, {}, Command(), id, id };
    std::cout << "[P" << id << "] Becoming CANDIDATE with ballot("
              << electionBallot.round << "," << id << ")\n";
    broadcastMessage(m);
    lastHeartbeat.store(clock::now());
}

void MultiPaxos::onElectionMessage(const Message &msg) {
    if (msg.type == MessageType::ELECTION) {
        // ignore lower ballots to maintain monotonicity
        if (msg.ballot < electionBallot) return;

        // vote only if we haven't voted for a higher ballot
        if (msg.ballot > votedForBallot) {
            votedForBallot = msg.ballot;
            Message voteMsg{ MessageType::VOTE, 0, {}, Command(), id, msg.candidateId };
            std::cout << "[P" << id << "] Voting for candidate P"
                      << msg.candidateId << " with ballot("
                      << msg.ballot.round << "," << msg.ballot.proposerId << ")\n";
            sendMessageTo(msg.senderId, voteMsg);
        }

    } else if (msg.type == MessageType::VOTE && msg.candidateId == id && leaderState == LeaderState::CANDIDATE) {
        if (electionVoteCount >= config.quorumSize && leaderState != LeaderState::LEADER) {
            becomeLeader();
        }

    } else if (msg.type == MessageType::LEADER_ANNOUNCE) {
        // another node became leader
        if (msg.candidateId != id) {
            becomeMember(msg.candidateId);
        }
    }
}

void MultiPaxos::becomeLeader() {
    leaderState = LeaderState::LEADER;
    leaderId = id;
    std::cout << "[P" << id << "] Became LEADER, announcing to cluster\n";

    // announce leadership
    Message la{ MessageType::LEADER_ANNOUNCE, 0, {}, Command(), id, id };
    broadcastMessage(la);
    currentBallot.round = std::max(currentBallot.round, electionBallot.round) + 1;
    currentBallot.proposerId = id;
    // send prepare for all pending slots to recover state
    for (int slot = 0; slot < nextSlot; ++slot) {
        if (!decidedCommands.count(slot)) {
        std::cout << "[P" << id << "] recovering slot=" << slot
                << " → send PREPARE ballot=("
                << currentBallot.round << "," << id << ")\n";
        sendPrepare(slot);
        } else {
            std::cout << "[P" << id << "] skipping already decided slot="
                << slot << "\n";
        }
}

    // start sending heartbeats immediately
    sendHeartbeat();
    lastHeartbeat.store(clock::now());
}

void MultiPaxos::becomeMember(int newLeaderId) {
    leaderState = LeaderState::MEMBER;
    leaderId = newLeaderId;
    std::cout << "[P" << id << "] Becoming MEMBER under leader P"
              << newLeaderId << "\n";
    lastHeartbeat.store(clock::now());
}

//------------------------------------------------------------------------------
// Prepare / Promise (Phase 1)
//------------------------------------------------------------------------------

void MultiPaxos::proposeCommand(int slot, const Command &cmd) {
    if (leaderState != LeaderState::LEADER) return;
    currentBallot.round++;
    currentBallot.proposerId = id;
    myProposals[slot] = cmd;
    auto &st = promiseStates[slot];
    st.count = 1;
    st.seenAnyAccepted = false;
    sendPrepare(slot);
}

void MultiPaxos::sendPrepare(int slot) {
    Message m{MessageType::PREPARE, slot, currentBallot, {}, id, 0};
    std::cout << "[P" << id << "] send PREPARE slot="<<slot
              <<" ballot=("<<m.ballot.round<<","<<m.ballot.proposerId<<")\n";
    broadcastMessage(m);
}

void MultiPaxos::onPrepare(const Message &msg) {
    auto &prom = promisedBallots[msg.slot];

    // ignore if we already promised a higher ballot
    if (msg.ballot < prom) {
        std::cout << "[P" << id << "] Rejected PREPARE for slot " << msg.slot
                  << " due to ballot (" << msg.ballot.round << "," << msg.ballot.proposerId
                  << ") < promised (" << prom.round << "," << prom.proposerId << ")\n";
        return;
    }

    // record promise
    prom = msg.ballot;

    // include any previously accepted command
    Command prev{};
    if (acceptedCommands.count(msg.slot)) {
        prev = acceptedCommands[msg.slot];
    }

    Message promiseMsg{ MessageType::PROMISE, msg.slot, msg.ballot, prev, id, 0 };
    std::cout << "[P" << id << "] Responding with PROMISE for slot "
              << msg.slot << "\n";
    sendMessageTo(msg.senderId, promiseMsg);
}

void MultiPaxos::onPromise(const Message &msg) {
    auto &st = promiseStates[msg.slot];
    if (st.count < 0) return;

    // count promises
    st.count++;
    std::cout << "[P" << id << "] Received PROMISE for slot "
              << msg.slot << ", count=" << st.count << "\n";

    // track highest accepted value if present
    if (msg.command.operation != Command::OpType::NOP) {
        if (!st.seenAnyAccepted || st.highestAcceptedBallot < msg.ballot) {
            st.highestAcceptedBallot = msg.ballot;
            st.highestAcceptedValue   = msg.command;
            st.seenAnyAccepted        = true;
        }
    }

    // once quorum, send accept
    if (st.count >= config.quorumSize) {
        st.count = -1;
        auto chosen = st.seenAnyAccepted ? st.highestAcceptedValue : myProposals[msg.slot];
        sendAccept(msg.slot, currentBallot, chosen);
    }
}

//------------------------------------------------------------------------------
// Accept / Accepted / Decide (Phase 2 & 3)
//------------------------------------------------------------------------------

void MultiPaxos::sendAccept(int slot, const Ballot &b, const Command &c) {
    auto &st = acceptStates[slot];
    st.count = 1;  // implicit “accept” from leader itself
    Message m{MessageType::ACCEPT, slot, b, c, id, 0};
    std::cout << "[P" << id << "] send ACCEPT slot="<<slot
            <<" ballot=("<<b.round<<","<<b.proposerId<<") cmd=("
            <<c.key<<","<<c.value<<"), self-count=1\n";
    broadcastMessage(m);
}

void MultiPaxos::onAccept(const Message &msg) {
    auto &prom = promisedBallots[msg.slot];
    if (!(msg.ballot < prom)) {
        prom = msg.ballot;
        acceptedBallots[msg.slot]=msg.ballot;
        acceptedCommands[msg.slot]=msg.command;
        Message ack{MessageType::ACCEPTED, msg.slot, msg.ballot,msg.command,id,0};
        std::cout << "[P" << id << "] onAccept from P"<<msg.senderId
                  <<" slot="<<msg.slot<<" → sending ACCEPTED\n";
        sendMessageTo(msg.senderId, ack);
    }
}


void MultiPaxos::onAccepted(const Message &msg) {
    // ack state
    auto &st = acceptStates[msg.slot];

    // count ack
    st.count++;
    std::cout << "[P" << id << "] Received ACCEPTED for slot="
              << msg.slot << ", count=" << st.count << "\n";

    // quorum reached
    if (st.count >= config.quorumSize) {

        Message dec{
            MessageType::DECIDE,
            msg.slot,
            msg.ballot,
            msg.command,
            id,
            0
        };
        std::cout << "[P" << id << "] Quorum reached for slot="
                  << msg.slot << " → broadcasting DECIDE\n";

        // broadcast decide
        broadcastMessage(dec);
        onDecide(dec);

        st.count = 0;
    }
}

void MultiPaxos::onDecide(const Message &msg) {
    decidedCommands[msg.slot] = msg.command;
    applyCommand(msg.slot, msg.command);
    nextSlot = std::max(nextSlot, msg.slot + 1);
    std::cout << "[P" << id << "] onDecide slot="<<msg.slot
              <<" cmd=("<<msg.command.key<<","<<msg.command.value<<") "
              <<"→ nextSlot="<<nextSlot<<"\n";
}
//------------------------------------------------------------------------------
// Heartbeat & failure detection
//------------------------------------------------------------------------------

void MultiPaxos::sendHeartbeat() {
    Message m{MessageType::HEARTBEAT, 0, currentBallot, {}, id, 0};
    std::cout << "[P" << id << "] send HEARTBEAT\n";
    broadcastMessage(m);
}

void MultiPaxos::onHeartbeat(const Message &msg) {
    if (msg.senderId == leaderId) {
        lastHeartbeat.store(clock::now());
    }
}

void MultiPaxos::checkHeartbeat() {

    Emu::Emulation::currentPid = id;
    using clock = std::chrono::steady_clock;
    while (!stop.load()) {
        auto now = clock::now();
        if (leaderState == LeaderState::LEADER) {
            sendHeartbeat();
        } else {
            auto since = now - lastHeartbeat.load();
            if (since > ELECTION_TIMEOUT) {
                becomeCandidate();
            }
        }
        std::this_thread::sleep_for(HEARTBEAT_INTERVAL);
    }
}

//------------------------------------------------------------------------------
// Dispatch helper
//------------------------------------------------------------------------------

void MultiPaxos::onPaxosMessage(const Message &msg) {

    switch(msg.type) {
      case MessageType::PREPARE:  onPrepare(msg);  break;
      case MessageType::PROMISE:  onPromise(msg);  break;
      case MessageType::ACCEPT:   onAccept(msg);   break;
      case MessageType::ACCEPTED: onAccepted(msg); break;
      case MessageType::DECIDE:   onDecide(msg);   break;
      default: break;
    }
}

//------------------------------------------------------------------------------
// State machine apply
//------------------------------------------------------------------------------

void MultiPaxos::applyCommand(int, const Command &cmd) {
    switch(cmd.operation) {
      case Command::OpType::ADD:
        if(!keyValueStore.count(cmd.key))
          keyValueStore[cmd.key] = cmd.value;
        break;
      case Command::OpType::UPDATE:
        if(keyValueStore.count(cmd.key))
          keyValueStore[cmd.key] = cmd.value;
        break;
      case Command::OpType::DELETE:
        keyValueStore.erase(cmd.key);
        break;
      default: break;
    }
}

//------------------------------------------------------------------------------
// Test‐only wrappers & accessors
//------------------------------------------------------------------------------
void MultiPaxos::processElectionMsg(const Message &m) { onElectionMessage(m); }
void MultiPaxos::processPromise   (const Message &m) { onPromise(m); }
void MultiPaxos::processAccepted  (const Message &m) { onAccepted(m); }
void MultiPaxos::processDecide    (const Message &m) { onDecide(m);    }

LeaderState MultiPaxos::getLeaderState() const   { return leaderState.load(); }
Ballot      MultiPaxos::getCurrentBallot() const { return currentBallot;  }

} // namespace Paxos