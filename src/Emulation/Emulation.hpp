#ifndef EMULATION_HPP
#define EMULATION_HPP

#include <boost/random.hpp>
#include <unordered_map>
#include <thread>
#include <queue>
#include <functional>
#include <unordered_set>
#include <mutex>
#include <random>
#include <future>
#include "../Message/Message.hpp"

class Emulation {
public:
    Emulation();
    ~Emulation();

    /**
     * Spawn a new process with the given function.
     * This method will create a new thread to run the process function.
     * @param fn The function to run for the process.
     */
    std::thread::id spawn(const std::function<void()>& fn);

    /**
     * Send a message to a receiver process.
     * If the receiver is reachable, the message is added to the receiver's message queue.
     * @param receiverPid The process ID of the receiver.
     * @param message The message to be sent.
     * @return True if the message was successfully queued, false otherwise
     */
    bool send(const std::thread::id receiverPid, std::unique_ptr<const Message> message);

    /**
     * Retrieves the next message from the current process's message queue.
     * @return The next message in the queue for the calling pid.
     */
    std::unique_ptr<const Message> receiveMessage();

    /**
     * Partitions the network, preventing processes in group A from communicating with processes in group B.
     * @param groupAPids A set of process IDs in group A.
     * @param groupBPids A set of process IDs in group B.
     * @return True if the partition was successful, false otherwise.
     */
    bool partitionNetwork(const std::unordered_set<std::thread::id>& groupAPids, const std::unordered_set<std::thread::id>& groupBPids);

    /**
     * Reconnects processes previously partitioned, allowing communication between group A and group B.
     * @param groupAPids A set of process IDs in group A.
     * @param groupBPids A set of process IDs in group B.
     * @return True if the connection was successful, false otherwise.
     */
    bool connectNetwork(const std::unordered_set<std::thread::id>& groupAPids, const std::unordered_set<std::thread::id>& groupBPids);

    /**
     * Temporarily "kills" or "revives" the process, marking it as inactive or active.
     * @param pid The process ID of the process to kill.
     */
    void setIsProcessDead(const std::thread::id pid, bool isDead);

    /**
    * @param pid The process ID of the process to kill.
    * @return True if the process was successfully killed, false otherwise.
    */
    bool isProcessDead(const std::thread::id pid) const;   

    /**
     * Sets averageMessageDelay for all processes using this Emulation.
     * @param delay the new delay in milli-seconds.
     */
    void setDelay(long delay);

private:
    std::unordered_map<std::thread::id, std::queue<std::unique_ptr<const Message>>> messageQueueMap;
    std::unordered_map<std::thread::id, std::unordered_set<std::thread::id>> reachableMap;
    std::unordered_map<std::thread::id, bool> isDeadMap;
    std::thread::id mainThread;
    int averageMessageDelay;

    mutable std::mutex messageQueueMapMutex;
    mutable std::mutex reachableMapMutex;
    mutable std::mutex isDeadMapMutex;
    mutable std::mutex averageMessageDelayMutex;

    std::random_device rd;
    boost::random::mt19937 gen;
    boost::random::exponential_distribution<> dist; 

    bool canSend(const std::thread::id receiverPid) const;
    double getDelay();
};

#endif
