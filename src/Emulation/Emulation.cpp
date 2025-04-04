#include "Emulation.hpp"
#include <sstream>

namespace Emu {

    // -------------------------
    // PairHash Implementation
    // -------------------------
    template <typename T, typename U>
    std::size_t PairHash::operator()(const std::pair<T, U>& p) const {
        auto h1 = std::hash<T>{}(p.first);
        auto h2 = std::hash<U>{}(p.second);
        return h1 ^ (h2 << 1);
    }

// -------------------------
// Thread-local variable Initialization
// -------------------------
thread_local Emulation::PID Emulation::currentPid = -1;

// -------------------------
// Emulation Constructor & Destructor
// -------------------------
Emulation::Emulation() : averageMessageDelay(1000) {
    // Additional initialization if necessary.
}

Emulation::~Emulation() {
    // Cleanup resources if necessary.
}

// -------------------------
// Process Management
// -------------------------
void Emulation::spawn(PID pid, ProcessFunction fn) {
    {
        std::lock_guard<std::mutex> lock(globalMutex);
        messageQueueMap[pid] = std::make_shared<ProcessQueue>();
        reachableMap[pid] = std::unordered_set<PID>(); 
        isDead[pid] = false;
    }
    std::thread t([=]() {
        currentPid = pid;
        fn();
    });
    t.detach();
}

Emulation::PID Emulation::whoami() {
    return currentPid;
}

// -------------------------
// Messaging Functions
// -------------------------
bool Emulation::send(PID receiverPid, const Message& message) {
    PID senderPid = currentPid;
    if (!canSend(senderPid, receiverPid)) {
        std::cout << "[Network] Message dropped from " << senderPid
                  << " to " << receiverPid << " (not reachable or dead)" << std::endl;
        return false;
    }
    double lossProb = 0.0;
    {
        std::lock_guard<std::mutex> lock(globalMutex);
        auto it = lossProbabilities.find({senderPid, receiverPid});
        if (it != lossProbabilities.end())
            lossProb = it->second;
    }
    if (shouldDrop(lossProb)) {
        std::cout << "[Network] Message lost from " << senderPid
                  << " to " << receiverPid << std::endl;
        return false;
    }
    int delay = averageMessageDelay;
    {
        std::lock_guard<std::mutex> lock(globalMutex);
        auto it = customDelays.find({senderPid, receiverPid});
        if (it != customDelays.end())
            delay = it->second;
    }
    int actualDelay = randomDelay(delay);
    std::this_thread::sleep_for(std::chrono::milliseconds(actualDelay));
    auto queuePtr = getProcessQueue(receiverPid);
    if (queuePtr) {
        std::lock_guard<std::mutex> lock(queuePtr->mtx);
        queuePtr->queue.push({senderPid, message});
        queuePtr->cv.notify_one();
        return true;
    }
    return false;
}

void Emulation::broadcast(const Message& message) {
    PID senderPid = currentPid;
    std::vector<PID> targets;
    {
        std::lock_guard<std::mutex> lock(globalMutex);
        auto it = reachableMap.find(senderPid);
        if (it != reachableMap.end()) {
            for (const auto& pid : it->second) {
                if (pid != senderPid)
                    targets.push_back(pid);
            }
        }
    }
    for (const auto& pid : targets) {
        send(pid, message);
    }
}

std::pair<Emulation::PID, Emulation::Message> Emulation::receiveMessage() {
    PID pid = currentPid;
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

bool Emulation::canSend(PID sender, PID receiver) {
    std::lock_guard<std::mutex> lock(globalMutex);
    if (isDead[sender] || isDead[receiver])
        return false;
    auto it = reachableMap.find(sender);
    if (it != reachableMap.end())
        return (it->second.find(receiver) != it->second.end());
    return false;
}

// -------------------------
// Network Control Functions
// -------------------------
void Emulation::partitionNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB) {
    std::lock_guard<std::mutex> lock(globalMutex);
    for (auto a : groupA) {
        for (auto b : groupB) {
            reachableMap[a].erase(b);
            reachableMap[b].erase(a);
        }
    }
    std::cout << "[Network] Partition applied between groups." << std::endl;
}

void Emulation::connectNetwork(const std::vector<PID>& groupA, const std::vector<PID>& groupB) {
    std::lock_guard<std::mutex> lock(globalMutex);
    for (auto a : groupA) {
        for (auto b : groupB) {
            reachableMap[a].insert(b);
            reachableMap[b].insert(a);
        }
    }
    std::cout << "[Network] Connection restored between groups." << std::endl;
}

// -------------------------
// Process State Control Functions
// -------------------------
void Emulation::killProcess(PID pid) {
    std::lock_guard<std::mutex> lock(globalMutex);
    isDead[pid] = true;
    std::cout << "[System] Killed process " << pid << std::endl;
}

void Emulation::reviveProcess(PID pid) {
    std::lock_guard<std::mutex> lock(globalMutex);
    isDead[pid] = false;
    std::cout << "[System] Revived process " << pid << std::endl;
}

// -------------------------
// Network Simulation Settings Functions
// -------------------------
void Emulation::injectNetworkDelay(PID pidA, PID pidB, int delayMs) {
    std::lock_guard<std::mutex> lock(globalMutex);
    customDelays[{pidA, pidB}] = delayMs;
}

void Emulation::injectNetworkLoss(PID pidA, PID pidB, double lossProbability) {
    std::lock_guard<std::mutex> lock(globalMutex);
    lossProbabilities[{pidA, pidB}] = lossProbability;
}

void Emulation::setAverageMessageDelay(int delayMs) {
    std::lock_guard<std::mutex> lock(globalMutex);
    averageMessageDelay = delayMs;
}

// -------------------------
// Internal Helper Functions
// -------------------------
std::shared_ptr<ProcessQueue> Emulation::getProcessQueue(PID pid) {
    std::lock_guard<std::mutex> lock(globalMutex);
    auto it = messageQueueMap.find(pid);
    if (it != messageQueueMap.end())
        return it->second;
    return nullptr;
}

bool Emulation::isProcessDead(PID pid) {
    std::lock_guard<std::mutex> lock(globalMutex);
    return isDead[pid];
}

int Emulation::randomDelay(int delay) {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(delay / 2, (delay * 3) / 2);
    return distribution(generator);
}

bool Emulation::shouldDrop(double probability) {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(generator) < probability;
}

} // namespace Emu