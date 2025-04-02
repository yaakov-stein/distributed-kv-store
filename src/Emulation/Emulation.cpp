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

struct PairHash {
    template <typename T, typename U>
    std::size_t operator()(const std::pair<T, U>& p) const {
        auto h1 = std::hash<T>{}(p.first);
        auto h2 = std::hash<U>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

// a message queue for each process
struct ProcessQueue {
    std::queue<std::pair<int, std::string>> queue;
    std::mutex mtx;
    std::condition_variable cv;
};

class Emulation {
    public:
        using PID = int;
        using Message = std::string;
        using ProcessFunction = std::function<void()>;
        Emulation() : averageMessageDelay(1000) {}

        // spawn(pid, fn)
        // generate process `pid`
        void spawn(PID pid, ProcessFunction fn) {
            {
                std::lock_guard<std::mutex> lock(globalMutex);
                // each process has its own message queue
                messageQueueMap[pid] = std::make_shared<ProcessQueue>();
                // each process has its own set of reachable processes
                // set up connection on connectNetwork
                reachableMap[pid] = {};
                isDead[pid] = false;
            }
            // create a new thread
            std::thread t([this, pid, fn]() {
                currentPid = pid;  // set up currentPid
                fn();              // do the work
            });
            t.detach();
        }
    
        // whoami()
        PID whoami() {
            return currentPid;
        }

        bool send(PID receiverPid, const Message& message) {
            PID senderPid = currentPid;
            // check if the message can be sent
            if (!canSend(senderPid, receiverPid)) {
                std::cout << "[Network] Message dropped from " << senderPid
                          << " to " << receiverPid << " (not reachable or dead)\n";
                return;
            }
    
            // drop a message by loss probability
            double lossProb = 0.0;
            {
                std::lock_guard<std::mutex> lock(globalMutex);
                auto it = lossProbabilities.find({senderPid, receiverPid});
                if (it != lossProbabilities.end())
                    lossProb = it->second;
            }
            if (shouldDrop(lossProb)) {
                std::cout << "[Network] Message lost from " << senderPid
                          << " to " << receiverPid << "\n";
                return;
            }
    
            // calculate delay
            int delay = averageMessageDelay;
            {
                std::lock_guard<std::mutex> lock(globalMutex);
                auto it = customDelays.find({senderPid, receiverPid});
                if (it != customDelays.end())
                    delay = it->second;
            }

    
            // sleep for the delay
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    
            // send the message into the queue
            auto queuePtr = getProcessQueue(receiverPid);
            if (queuePtr) {
                std::lock_guard<std::mutex> lock(queuePtr->mtx);
                queuePtr->queue.push({senderPid, message});
                queuePtr->cv.notify_one();
                return true;
            }
            return false;

        }

        std::pair<PID, Message> receiveMessage() {
            PID pid = currentPid;
            // wait til process is alive
            while (isProcessDead(pid)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            auto queuePtr = getProcessQueue(pid);
            std::unique_lock<std::mutex> lock(queuePtr->mtx);
            queuePtr->cv.wait(lock, [&]{ return !queuePtr->queue.empty(); });
            auto msg = queuePtr->queue.front();
            queuePtr->queue.pop();
            return msg;
        }
        // broadcast(message)
        // current process broadcasts a message to all reachable processes
        void broadcast(const Message& message) {
            PID senderPid = currentPid;
            std::vector<PID> targets;
            {
                std::lock_guard<std::mutex> lock(globalMutex);
                // reachableMap[senderPid] only
                for (PID pid : reachableMap[senderPid]) {
                    if (pid != senderPid)
                        targets.push_back(pid);
                }
            }
            for (PID pid : targets) {
                send(pid, message);
            }
        }
        // canSend(sender, receiver)
        bool canSend(PID sender, PID receiver) {
            std::lock_guard<std::mutex> lock(globalMutex);
            if (isDead[sender] || isDead[receiver])
                return false;
            // reachableMap[sender] has to contain a receiver
            return (reachableMap[sender].find(receiver) != reachableMap[sender].end());
        }
        void killProcess(PID pid) {
            isDead[pid] = true;
            std::cout << "[System] Killed process " << pid << "\n";
        }

        void reviveProcess(PID pid) {
            isDead[pid] = false;
            std::cout << "[System] Revived process " << pid << "\n";
        }

        void partitionNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB) {
            for (PID a : groupA) for (PID b : groupB) {
                reachableMap[a].erase(b);
                reachableMap[b].erase(a);
            }
            std::cout << "[Network] Partition applied between groups.\n";
        }

        void connectNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB) {
            for (PID a : groupA) for (PID b : groupB) {
                reachableMap[a].insert(b);
                reachableMap[b].insert(a);
            }
            std::cout << "[Network] Connection restored between groups.\n";
        }

        void injectNetworkDelay(PID a, PID b, int delayMs) {
            customDelays[{a, b}] = delayMs;
        }
    
        void injectNetworkLoss(PID a, PID b, double lossProbability) {
            lossProbabilities[{a, b}] = lossProbability;
        }
    
        void setAverageMessageDelay(int delayMs) {
            averageMessageDelay = delayMs;
        }
        private:
    
        // messeageque for each process
        std::unordered_map<PID, std::shared_ptr<ProcessQueue>> messageQueueMap;
        // reachable maps
        std::unordered_map<PID, std::unordered_set<PID>> reachableMap;
        // process status
        std::unordered_map<PID, bool> isDead;
    
        std::unordered_map<std::pair<PID, PID>, int, PairHash> customDelays;
        std::unordered_map<std::pair<PID, PID>, double, PairHash> lossProbabilities;
    
        // access global var
        std::mutex globalMutex;
        // 
        std::unordered_map<PID, std::mutex> queueMutex;
        // average message delay
        int averageMessageDelay;
        
        // current process id
        static thread_local PID currentPid;

        bool isProcessDead(PID pid) {
            std::lock_guard<std::mutex> lock(globalMutex);
            return isDead[pid];
        }
        // get a process queue
        std::shared_ptr<ProcessQueue> getProcessQueue(PID pid) {
            std::lock_guard<std::mutex> lock(globalMutex);
            auto it = messageQueueMap.find(pid);
            if (it != messageQueueMap.end())
                return it->second;
            return nullptr;
        }
        // random delay between 50% to 150% of the specified delay time
        int randomDelay(int delay) {
            static thread_local std::mt19937 generator(std::random_device{}());
            std::uniform_int_distribution<int> distribution(delay / 2, (delay * 3) / 2);
            return distribution(generator);
        }

        // judge if the message should be dropped
        bool shouldDrop(double probability) {
            static thread_local std::mt19937 generator(std::random_device{}());
            std::uniform_real_distribution<double> distribution(0.0, 1.0);
            return distribution(generator) < probability;
        }
};


thread_local Emulation::PID Emulation::currentPid = -1;
