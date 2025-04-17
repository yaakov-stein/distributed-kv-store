#ifndef MULTIPAXOS_HPP
#define MULTIPAXOS_HPP

#include "Emulation.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>

// ------------------------------------------------------------------
// Global Types and Structures
// ------------------------------------------------------------------

// MessageType
enum class MessageType {
    PREPARE,
    PROMISE,
    ACCEPT,
    ACCEPTED,
    DECIDE,
    ELECTION,
    VOTE,
    LEADER_ANNOUNCE,
    HEARTBEAT
};

// Ballot: ProposerId and Round
struct Ballot {
    int round;
    int proposerId;

    bool operator<(const Ballot &other) const {
        if (round == other.round)
            return proposerId < other.proposerId;
        return round < other.round;
    }
};

// Command: Key-Value ops
struct Command {
    enum class OpType { ADD, DELETE, UPDATE, GET, NOP };
    OpType operation;
    std::string key;
    int value;
    // init for nop
    Command() : operation(OpType::NOP), key(), value(0) {}
    Command(OpType op, const std::string &k, int v = 0)
        : operation(op), key(k), value(v) {}
};

// Message: Paxos messages on each phase
struct Message {
    MessageType type;
    int slot;       // slot number
    Ballot ballot;  // propose id and round
    Command command;// Command on Accept/Decide 
    int senderId;
    int candidateId; // ELECTION, VOTE, LEADER_ANNOUNCE
};

// Configuration: 
struct Configuration {
    std::vector<int> peers;  // all prosess ids
    int quorumSize;

    Configuration(const std::vector<int>& peers_);
};

// LeaderState: member, candidate, leader
enum class LeaderState {
    MEMBER,
    CANDIDATE,
    LEADER
};


// ------------------------------------------------------------------
// MultiPaxos Class Declaration
// ------------------------------------------------------------------
class MultiPaxos {
    public:
        // constructor 
        MultiPaxos(int id, const Configuration &config);
    
        // receipt of a message
        void run();
    
        // proposeCommand only for leader
        void proposeCommand(int slot, const Command &cmd);
    
        // shutdown: kill a processs
        void shutdown();
    
        // getState:return key-value store state
        std::unordered_map<std::string, int> getState() const;
    
    private:
        // p id
        int id;
        // config
        Configuration config;
        // current state of the process, leader, candidate or member
        std::atomic<LeaderState> leaderState;
        // current leader id
        int leaderId;
        // current proposal ballot, proposer id and round
        Ballot currentBallot;
    
        // key-value store
        std::unordered_map<std::string, int> keyValueStore;
    
        // Paxos internal states
        std::unordered_map<int, Ballot> promisedBallots;
        std::unordered_map<int, Ballot> acceptedBallots;
        std::unordered_map<int, Command> acceptedCommands;
        std::unordered_map<int, Command> decidedCommands;
    
        // stop flag
        std::atomic<bool> stop;
    
        // ------------------------------------------------------------------
        // Paxos Phase 1/2 Methods
        // ------------------------------------------------------------------
        void sendPrepare(int slot);
        void onPrepare(const Message &msg);
        void onPromise(const Message &msg);
        void sendAccept(int slot, const Ballot &ballot, const Command &cmd);
        void onAccept(const Message &msg);
        void onAccepted(const Message &msg);
        void onDecide(const Message &msg);
    
        // ------------------------------------------------------------------
        // Leader Election Methods
        // ------------------------------------------------------------------
        void startElection();
        void becomeCandidate();
        void onElectionMessage(const Message &msg);
        void becomeLeader();
        void becomeMember(int newLeaderId);
    
        // ------------------------------------------------------------------
        // Failure Recovery and Heartbeat
        // ------------------------------------------------------------------
        void sendHeartbeat();
        void checkHeartbeat();
    
        // ------------------------------------------------------------------
        // Utility: Apply Decided Command to Key-Value Store
        // ------------------------------------------------------------------
        void applyCommand(int slot, const Command &cmd);
    
        // ------------------------------------------------------------------
        // Emulation Send/Receive Wrappers (テンプレート利用可能)
        // ------------------------------------------------------------------
        template<typename T>
        void sendMessageTo(int targetId, const T &msg);
        template<typename T>
        void broadcastMessage(const T &msg);
    
        // ------------------------------------------------------------------
        // Internal Message Dispatchers
        // ------------------------------------------------------------------
        void onPaxosMessage(const Message &msg);
        void onHeartbeat(const Message &msg);
    };
    
    #endif  // MULTIPAXOS_HPP