#ifndef EMULATION_HPP
#define EMULATION_HPP

#include<unordered_map>
#include<string>
#include<queue>
#include<vector>
#include<functional>
#include<unordered_set>
#include<mutex>

class Emulation {
public:
    Emulation();
    ~Emulation();

    /**
     * Spawn a new process with the given process ID and function.
     * This method will create a new thread to run the process function.
     * @param pid The process ID (a unique identifier for the process).
     * @param fn The function to run for the process.
     */
    void spawn(const std::string& pid, const std::function<void()> fn);

    /**
     * Send a message to a receiver process.
     * If the receiver is alive and reachable, the message is added to the receiver's message queue.
     * @param receiverPid The process ID of the receiver.
     * @param message The message to be sent.
     * @return True if the message was successfully queued, false otherwise.
     */
    bool send(const std::string& receiverPid, const std::string& message);

    /**
     * Partitions the network, preventing processes in group A from communicating with processes in group B.
     * @param groupAPids A set of process IDs in group A.
     * @param groupBPids A set of process IDs in group B.
     * @return True if the partition was successful, false otherwise.
     */
    bool partitionNetwork(const std::unordered_set<std::string> groupAPids, const std::unordered_set<std::string> groupBPids);

    /**
     * Temporarily "kills" the process, marking it as inactive.
     * @param pid The process ID of the process to kill.
     * @return True if the process was successfully killed, false otherwise.
     */
    bool killProcess(const std::string& pid);

    /**
     * Revives a previously killed process, marking it as active again.
     * @param pid The process ID of the process to revive.
     * @return True if the process was successfully revived, false otherwise.
     */
    bool reviveProcess(const std::string& pid);

    /**
     * Reconnects processes previously partitioned, allowing communication between group A and group B.
     * @param groupAPids A set of process IDs in group A.
     * @param groupBPids A set of process IDs in group B.
     * @return True if the connection was successful, false otherwise.
     */
    bool connectNetwork(const std::unordered_set<std::string> groupAPids, const std::unordered_set<std::string> groupBPids);

    /**
     * Retrieves the next message from the current process's message queue.
     * @return The next message in the queue for the calling pid.
     */
    const std::string& receiveMessage();

    /**
     * Sets averageMessageDelay for all processes using this Emulation.
     * @param delay the new delay in milli-seconds.
     */
    void setDelay(int delay);

private:
    std::unordered_map<std::string, std::queue<std::string>> messageQueueMap;
    std::unordered_map<std::string, std::vector<std::string>> reachableMap;
    std::unordered_map<std::string, bool> isDead;
    int averageMessageDelay;

    mutable std::mutex messageQueueMapMutex;
    mutable std::mutex reachableMapMutex;
    mutable std::mutex isDeadMutex;
    mutable std::mutex averageMessageDelayMutex;

    bool canSend(const std::string& receiverPid) const;
};

#endif
