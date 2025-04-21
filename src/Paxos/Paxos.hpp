// src/Paxos/Paxos.hpp
#ifndef PAXOS_HPP
#define PAXOS_HPP

#include "Emulation.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>

namespace Paxos {

// ------------------------------------------------------------------
// Types of messages exchanged in MultiPaxos
// ------------------------------------------------------------------
enum class MessageType {
    PREPARE,
    PROMISE,
    ACCEPT,
    ACCEPTED,
    DECIDE,
    CLIENT,
    ELECTION,
    VOTE,
    LEADER_ANNOUNCE,
    HEARTBEAT
};

// ------------------------------------------------------------------
// A Paxos ballot: (round, proposerId), lexicographically ordered
// ------------------------------------------------------------------
struct Ballot {
    int round;
    int proposerId;
    bool operator<(const Ballot &o) const {
        if (round == o.round) return proposerId < o.proposerId;
        return round < o.round;
    }
};

// ------------------------------------------------------------------
// A single key‑value command
// ------------------------------------------------------------------
struct Command {
    enum class OpType { ADD, DELETE, UPDATE, GET, NOP };
    OpType operation;
    std::string key;
    int value;
    Command() : operation(OpType::NOP), key(), value(0) {}
    Command(OpType op, const std::string &k, int v = 0)
      : operation(op), key(k), value(v) {}
};

// ------------------------------------------------------------------
// A Paxos protocol message
// ------------------------------------------------------------------
struct Message {
    MessageType type;
    int slot;        // log slot index
    Ballot ballot;   // ballot number
    Command command; // for ACCEPT/DECIDE or GET
    int senderId;    // sender process ID
    int candidateId; // for ELECTION/VOTE
};

// ------------------------------------------------------------------
// Cluster configuration: peer IDs + quorum size
// ------------------------------------------------------------------
struct Configuration {
    std::vector<int> peers;
    int quorumSize;
    Configuration(const std::vector<int> &ps)
      : peers(ps), quorumSize(int(ps.size())/2 + 1) {}
};

// ------------------------------------------------------------------
// Role of this node in leader election
// ------------------------------------------------------------------
enum class LeaderState {
    MEMBER,    // follower
    CANDIDATE, // running for leader
    LEADER     // leader
};

// ------------------------------------------------------------------
// MultiPaxos: a single node with built‑in leader election
// ------------------------------------------------------------------
class MultiPaxos {
  public:
    MultiPaxos(int id, const Configuration &config, Emu::Emulation* emulator);

    // Main loop: receives messages from Emulation, dispatches
    void run();
    // Client-facing proposal (leader only)
    void proposeCommand(int slot, const Command &cmd);
    void shutdown();
    // Inspect state machine
    std::unordered_map<std::string,int> getState() const;

    // ----------------------------------------------------------------
    // Public hooks for tests and external control
    // ----------------------------------------------------------------
    std::atomic<LeaderState> leaderState;
    Ballot                  currentBallot;

    // Leader election control
    void becomeCandidate();
    void becomeLeader();
    void becomeMember(int newLeaderId);
    void checkHeartbeat();

    // Phase‐0: Election messages
    void onElectionMessage(const Message &msg);
    // Phase‐1: Prepare/Promise
    void onPromise       (const Message &msg);
    // Phase‐2: Accept/Accepted
    void onAccepted      (const Message &msg);
    // Phase‐3: Decide
    void onDecide        (const Message &msg);

    // Direct API for tests
    LeaderState getLeaderState() const; // extract leader state for kv-store
    Ballot      getCurrentBallot() const;
    void processElectionMsg(const Message &msg);
    void processPromise   (const Message &msg);
    void processAccepted  (const Message &msg);
    void processDecide    (const Message &msg);

  private:
    Emu::Emulation* em; 
    int id;
    Configuration config;
    int leaderId;
    std::unordered_map<std::string,int> keyValueStore;

    // Paxos bookkeeping:
    std::unordered_map<int,Ballot>   promisedBallots;
    std::unordered_map<int,Ballot>   acceptedBallots;
    std::unordered_map<int,Command>  acceptedCommands;
    std::unordered_map<int,Command>  decidedCommands;

    struct PromiseState {
        int   count                = 0;               // start at zero
        bool  seenAnyAccepted      = false;           // none accepted yet
        Ballot highestAcceptedBallot{};               // default {0,0}
        Command highestAcceptedValue{};               // default nop
    };
    std::unordered_map<int,PromiseState> promiseStates;
    std::unordered_map<int,Command>      myProposals;
    std::unordered_map<int,int>          electionVotes;
    std::unordered_map<int, bool> votedFor;

    std::atomic<bool> stop;
    int               nextSlot;

    // Helpers for Paxos phases
    void onPrepare   (const Message &msg);
    void sendPrepare (int slot);
    void onAccept    (const Message &msg);
    void sendAccept  (int slot, const Ballot &b, const Command &c);

    // Heartbeat
    void sendHeartbeat();
    void onHeartbeat(const Message &msg);
    std::atomic<std::chrono::steady_clock::time_point> lastHeartbeat;
    // tuning parameters for heartbeats / timeouts
    static constexpr auto HEARTBEAT_INTERVAL = std::chrono::milliseconds(1000);
    static constexpr auto ELECTION_TIMEOUT  = std::chrono::milliseconds(10000);

    // State machine apply
    void applyCommand(int slot, const Command &cmd);

    // Dispatch all Paxos messages
    void onPaxosMessage(const Message &msg);

    // Emulation serialization
    template<typename T>
    void sendMessageTo(int target, const T &msg);
    template<typename T>
    void broadcastMessage(const T &msg);
    
};
std::string serializeMessage(const Message &m);
Message deserializeMessage(const std::string &raw);
} // namespace Paxos

#endif // PAXOS_HPP