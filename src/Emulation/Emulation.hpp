#ifndef EMULATION_HPP
#define EMULATION_HPP

#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <random>
#include <vector>
#include <memory>
#include <string>

namespace Emu {

/** 
 * @brief Hash function for std::pair<int, int>.
 */
struct PairHash {
    template <typename T, typename U>
    std::size_t operator()(const std::pair<T, U>& p) const;
};

/**
 * @brief Structure representing a message queue for each process.
 */
struct ProcessQueue {
    // Queue storing pairs of <sender process ID, message>
    std::queue<std::pair<int, std::string>> queue;
    std::mutex mtx;                      // Mutex to protect the queue
    std::condition_variable cv;          // Condition variable for waiting on messages
};

/**
 * @brief Emulation class simulates a distributed system with asynchronous communication.
 *
 * Provides functions for process spawning, message sending/receiving,
 * network simulation (delay, loss, partitioning), and process state control.
 */
class Emulation {
public:
    using PID = int;                     ///< Process identifier type.
    using Message = std::string;         ///< Message type.
    using ProcessFunction = std::function<void()>; ///< Function executed by a process.

    // Constructors & Destructor
    Emulation();
    ~Emulation();

    // Process Management
    void spawn(PID pid, ProcessFunction fn);
    PID whoami();

    // Messaging Functions
    bool send(PID receiverPid, const Message& message);
    void broadcast(const Message& message);
    std::pair<PID, Message> receiveMessage();
    bool canSend(PID sender, PID receiver);

    // Network Control
    void partitionNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB);
    void connectNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB);

    // Process State Control
    void killProcess(PID pid);
    void reviveProcess(PID pid);

    // Network Simulation Settings
    void injectNetworkDelay(PID pidA, PID pidB, int delayMs);
    void injectNetworkLoss(PID pidA, PID pidB, double lossProbability);
    void setAverageMessageDelay(int delayMs);

    // Each thread maintains its current process ID.
    static thread_local PID currentPid;

private:
    // Internal Data Structures
    std::unordered_map<PID, std::shared_ptr<ProcessQueue>> messageQueueMap;
    std::unordered_map<PID, std::unordered_set<PID>> reachableMap;
    std::unordered_map<PID, bool> isDead;
    std::unordered_map<std::pair<PID, PID>, int, PairHash> customDelays;
    std::unordered_map<std::pair<PID, PID>, double, PairHash> lossProbabilities;
    std::mutex globalMutex;
    int averageMessageDelay;

    // Internal Helper Functions
    std::shared_ptr<ProcessQueue> getProcessQueue(PID pid);
    bool isProcessDead(PID pid);
    int randomDelay(int delay);
    bool shouldDrop(double probability);
};

} // namespace Emu

#endif // EMULATION_HPP
