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

// Hash function for std::pair<int, int>
struct PairHash {
    template <typename T, typename U>
    std::size_t operator()(const std::pair<T, U>& p) const {
        auto h1 = std::hash<T>{}(p.first);
        auto h2 = std::hash<U>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

// Structure representing a message queue for each process.
struct ProcessQueue {
    // Queue storing pairs of <sender process ID, message>
    std::queue<std::pair<int, std::string>> queue;
    std::mutex mtx;                      // Mutex to protect the queue
    std::condition_variable cv;          // Condition variable for waiting on messages
};

class Emulation {
public:
    using PID = int;                     // Type alias for Process ID
    using Message = std::string;         // Type alias for messages
    using ProcessFunction = std::function<void()>; // Function type for process execution

    /**
     * @brief Constructor that initializes the average message delay.
     *
     * Sets the average message delay (in milliseconds) to 1000ms.
     */
    Emulation();

    /**
     * @brief Destructor.
     */
    ~Emulation();

    /**
     * @brief Spawns a new process with the given process ID and function.
     *
     * This method creates a new thread that runs the specified process function.
     *
     * @param pid The unique process identifier.
     * @param fn The function to be executed in the new process.
     */
    void spawn(PID pid, ProcessFunction fn);

    /**
     * @brief Returns the current process ID.
     *
     * @return The process ID of the calling thread.
     */
    PID whoami();

    /**
     * @brief Sends a message to a receiver process.
     *
     * If the receiver is alive and reachable, the message is added to the receiver's message queue.
     * The function simulates network delay and potential message loss.
     *
     * @param receiverPid The process ID of the receiver.
     * @param message The message to send.
     */
    void send(PID receiverPid, const Message& message);

    /**
     * @brief Broadcasts a message to all processes reachable from the current process.
     *
     * @param message The message to be broadcasted.
     */
    void broadcast(const Message& message);

    /**
     * @brief Retrieves the next message from the current process's message queue.
     *
     * If the process is "killed" (inactive), the call will wait until the process is revived.
     *
     * @return A pair consisting of the sender's process ID and the message.
     */
    std::pair<PID, Message> receiveMessage();

    /**
     * @brief Checks if a message can be sent between a sender and receiver.
     *
     * This considers both network connectivity (reachableMap) and the alive status of the processes.
     *
     * @param sender The sender's process ID.
     * @param receiver The receiver's process ID.
     * @return True if the message can be sent, false otherwise.
     */
    bool canSend(PID sender, PID receiver);

    /**
     * @brief Partitions the network between two groups of processes.
     *
     * Removes the connection between each process in groupA and each process in groupB.
     *
     * @param groupA A vector of process IDs in group A.
     * @param groupB A vector of process IDs in group B.
     */
    void partitionNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB);

    /**
     * @brief Connects two groups of processes by restoring communication.
     *
     * Adds bi-directional connectivity between each process in groupA and each process in groupB.
     *
     * @param groupA A vector of process IDs in group A.
     * @param groupB A vector of process IDs in group B.
     */
    void connectNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB);

    /**
     * @brief Kills (deactivates) the specified process.
     *
     * Marks the process as inactive so that it will not send or receive messages.
     *
     * @param pid The process ID to be killed.
     */
    void killProcess(PID pid);

    /**
     * @brief Revives a previously killed process.
     *
     * Marks the process as active so that it can resume sending and receiving messages.
     *
     * @param pid The process ID to be revived.
     */
    void reviveProcess(PID pid);

    /**
     * @brief Sets a custom network delay for messages from one process to another.
     *
     * Overrides the default average delay for messages sent from pidA to pidB.
     *
     * @param pidA The sender's process ID.
     * @param pidB The receiver's process ID.
     * @param delayMs The custom delay in milliseconds.
     */
    void injectNetworkDelay(PID pidA, PID pidB, int delayMs);

    /**
     * @brief Sets the loss probability for messages from one process to another.
     *
     * A message from pidA to pidB may be dropped with the specified probability.
     *
     * @param pidA The sender's process ID.
     * @param pidB The receiver's process ID.
     * @param lossProbability A value between 0.0 and 1.0 representing the drop probability.
     */
    void injectNetworkLoss(PID pidA, PID pidB, double lossProbability);

    /**
     * @brief Sets the average message delay for all processes.
     *
     * @param delayMs The new average delay in milliseconds.
     */
    void setAverageMessageDelay(int delayMs);

    // Each thread maintains its current process ID.
    static thread_local PID currentPid;

private:
    // Map of message queues for each process.
    std::unordered_map<PID, std::shared_ptr<ProcessQueue>> messageQueueMap;
    // Map of reachable processes for each process.
    std::unordered_map<PID, std::unordered_set<PID>> reachableMap;
    // Map of process statuses (true if the process is killed/inactive).
    std::unordered_map<PID, bool> isDead;
    // Map for custom delays (in ms) for (sender, receiver) pairs.
    std::unordered_map<std::pair<PID, PID>, int, PairHash> customDelays;
    // Map for message loss probabilities for (sender, receiver) pairs.
    std::unordered_map<std::pair<PID, PID>, double, PairHash> lossProbabilities;
    // Mutex to protect global data.
    std::mutex globalMutex;
    // Average message delay in milliseconds.
    int averageMessageDelay;

    /**
     * @brief Retrieves the message queue pointer for the specified process.
     *
     * @param pid The process ID.
     * @return A shared pointer to the process's message queue.
     */
    std::shared_ptr<ProcessQueue> getProcessQueue(PID pid);

    /**
     * @brief Checks whether the specified process is killed/inactive.
     *
     * @param pid The process ID.
     * @return True if the process is inactive, false otherwise.
     */
    bool isProcessDead(PID pid);

    /**
     * @brief Generates a random delay based on the provided delay value.
     *
     * The delay will be randomized between 50% and 150% of the given delay.
     *
     * @param delay The base delay value in milliseconds.
     * @return A randomized delay value in milliseconds.
     */
    int randomDelay(int delay);

    /**
     * @brief Determines if a message should be dropped based on the given probability.
     *
     * @param probability A value between 0.0 and 1.0 representing the drop probability.
     * @return True if the message should be dropped, false otherwise.
     */
    bool shouldDrop(double probability);
};

#endif // EMULATION_HPP