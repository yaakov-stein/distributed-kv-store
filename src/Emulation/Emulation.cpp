#include "Emulation.hpp"
#include "../Util.hpp"
#include <thread>
#include <unordered_set>

Emulation::Emulation() 
    : messageQueueMap(),
    reachableMap(),
    isDeadMap(),
    gen(rd()),
    averageMessageDelay(100),
    dist(1.0 / averageMessageDelay) 
    {
        this->mainThread = std::this_thread::get_id();

        {
            std::lock_guard<std::mutex> lock(this->messageQueueMapMutex);
            this->messageQueueMap.emplace(this->mainThread, std::queue<std::unique_ptr<const Message>>());
        }

        // Make sure thread is reachable by all other processes
        std::unordered_set<std::thread::id> allKeys;
        {
            std::lock_guard<std::mutex> lock(this->reachableMapMutex);
            allKeys = Util::getKeys(this->reachableMap);
        }
        allKeys.insert(this->mainThread);
        this->connectNetwork(allKeys, {this->mainThread});

        std::cout << "[Emulation::init] starting to run fn for thread id: " << std::this_thread::get_id() << std::endl;
    }
Emulation::~Emulation() {}

std::thread::id Emulation::spawn(const std::function<void()>& fn)
{
    auto startPromise = std::make_shared<std::promise<void>>();
    std::future<void> startFuture = startPromise->get_future();

    std::thread t([fn, startFuture=std::move(startFuture)]() {
        // Wait until main thread tells us to start
        startFuture.wait();
        std::cout << "[thread::fn] starting to run fn for thread id: " << std::this_thread::get_id() << std::endl;
        fn();
    });

    std::thread::id tid = t.get_id();

    // Add the thread with an empty queue to the message map
    {
        std::lock_guard<std::mutex> lock(this->messageQueueMapMutex);
        this->messageQueueMap.emplace(tid, std::queue<std::unique_ptr<const Message>>());
    }

    // Make sure thread is reachable by all other processes
    std::unordered_set<std::thread::id> allKeys;
    {
        std::lock_guard<std::mutex> lock(this->reachableMapMutex);
        allKeys = Util::getKeys(this->reachableMap);
    }
    allKeys.insert(tid);
    this->connectNetwork(allKeys, {tid});

    std::cout << "[Emulation::spawn] Completed Emulation setup for thread id: " << tid << std::endl;

    // Tell the thread it's safe to run now
    startPromise->set_value();
    t.detach();

    return tid;
}

// bool Emulation::send(const std::thread::id& receiverPid, std::unique_ptr<const Message> message)
// {
//     if(this->canSend(receiverPid)) 
//     {
//         std::thread([this, receiverPid, msg=std::move(message)]() mutable
//         {
//             const long delay_ms = static_cast<long>(this->getDelay());
//             std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
//             std::lock_guard<std::mutex> lock(this->messageQueueMapMutex);
//             auto it = this->messageQueueMap.find(receiverPid);
//             if(it != this->messageQueueMap.end())
//             {
//                 it->second.emplace(std::move(msg));
//             }
//         }).detach();
//         return true;
//     }
//     return false;
// }

bool Emulation::send(const std::thread::id receiverPid, std::unique_ptr<const Message> message)
{
    std::cout << "[Emulation::send] Attempting to send message to PID: " << receiverPid << std::endl;
    if (this->canSend(receiverPid)) 
    {
        std::cout << "[Emulation::send] Can send to PID: " << receiverPid << std::endl;
        std::thread([this, receiverPid, msg = std::move(message)]() mutable
        {
            const long delay_ms = static_cast<long>(this->getDelay());
            std::cout << "[Emulation::send Thread] Sleeping for " << delay_ms << " ms before sending." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            std::lock_guard<std::mutex> lock(this->messageQueueMapMutex);
            auto it = this->messageQueueMap.find(receiverPid);
            if (it != this->messageQueueMap.end())
            {
                std::cout << "[Emulation::send Thread] Found receiver. Enqueuing message." << std::endl;
                it->second.emplace(std::move(msg));
                std::cout << "[Emulation::send Thread] Found receiver. Message Enqueued." << std::endl;
            }
            else
            {
                std::cout << "[Emulation::send Thread] ERROR: Receiver PID not found in messageQueueMap!" << std::endl;
            }
        }).detach();
        return true;
    }
    else
    {
        std::cout << "[Emulation::send] Cannot send to PID: " << receiverPid << std::endl;
    }
    return false;
}

// bool Emulation::canSend(const std::thread::id& receiverPid) const
// {
//     std::thread::id this_id = std::this_thread::get_id();
//     {
//         std::lock_guard<std::mutex> lock(this->reachableMapMutex);
//         auto set = this->reachableMap.find(this_id);
//         if(set != this->reachableMap.end() && set->second.contains(receiverPid))
//         {
//             return true;
//         }
//     }
//     return false;
// }

bool Emulation::canSend(const std::thread::id receiverPid) const
{
    std::thread::id this_id = std::this_thread::get_id();
    {
        std::lock_guard<std::mutex> lock(this->reachableMapMutex);

        std::cout << "[canSend] Checking if current thread (" << this_id << ") can send to receiver (" << receiverPid << ")\n";
        std::cout << "[canSend] reachableMap contains " << this->reachableMap.size() << " entries.\n";

        for (const auto& [key, val] : reachableMap) {
            for (const auto& node:val) {
                std::cout << "[canSend] reachableMap key: " << key << ". val: " << node << "\n";
            }
        }
        
        auto set = this->reachableMap.find(this_id);
        if(set != this->reachableMap.end() && set->second.contains(receiverPid))
        {
            std::cout << "[canSend] Found current thread ID in reachableMap. Sending allowed.\n";
            return true;
        }
        else
        {
            std::cout << "[canSend] Current thread ID NOT found in reachableMap. Sending NOT allowed.\n";
        }
    }
    return false;
}

std::unique_ptr<const Message> Emulation::receiveMessage()
{
    std::thread::id tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(this->messageQueueMapMutex);
    auto it = this->messageQueueMap.find(tid);
    if(it == this->messageQueueMap.end() || it->second.empty())
    {
        return nullptr;
    }
    else
    {
        std::queue<std::unique_ptr<const Message>>& m_queue = it->second;
        std::unique_ptr<const Message> msg = std::move(m_queue.front());
        m_queue.pop();
        return msg;
    }
}

bool Emulation::partitionNetwork(const std::unordered_set<std::thread::id>& groupAPids, const std::unordered_set<std::thread::id>& groupBPids)
{
    std::lock_guard<std::mutex> lock(this->reachableMapMutex);
    for(auto& aPid : groupAPids)
    {
        for(auto& bPid : groupBPids)
        {
            auto itA = this->reachableMap.find(aPid);
            if(itA != this->reachableMap.end())
            {
                std::unordered_set<std::thread::id>& connectedToA = itA->second;
                connectedToA.erase(bPid);
            }

            auto itB = this->reachableMap.find(bPid);
            if(itB != this->reachableMap.end())
            {
                std::unordered_set<std::thread::id>& connectedToB = itB->second;
                connectedToB.erase(aPid);
            }
        }
    }
    return true;
}

bool Emulation::connectNetwork(const std::unordered_set<std::thread::id>& groupAPids, const std::unordered_set<std::thread::id>& groupBPids)
{
    std::lock_guard<std::mutex> lock(this->reachableMapMutex);
    for(auto& aPid : groupAPids)
    {
        for(auto& bPid : groupBPids)
        {
            auto itA = this->reachableMap.find(aPid);
            if(itA == this->reachableMap.end())
            {
                this->reachableMap.insert({aPid, {}});
                itA = this->reachableMap.find(aPid);
            }
            std::unordered_set<std::thread::id>& connectedToA = itA->second;
            connectedToA.insert(bPid);

            auto itB = this->reachableMap.find(bPid);
            if(itB == this->reachableMap.end())
            {
                this->reachableMap.insert({bPid, {}});
                itA = this->reachableMap.find(bPid);
            }
            std::unordered_set<std::thread::id>& connectedToB = itB->second;
            connectedToB.insert(aPid);
        }
    }
    return true;   
}

void Emulation::setIsProcessDead(const std::thread::id pid, bool isDead)
{
    std::lock_guard<std::mutex> lock(this->isDeadMapMutex);
    this->isDeadMap[pid] = isDead;
}

bool Emulation::isProcessDead(const std::thread::id pid) const
{
    std::lock_guard<std::mutex> lock(this->isDeadMapMutex);
    auto it = this->isDeadMap.find(pid);
    if(it == this->isDeadMap.end())
        return false;
    return it->second;
}

void Emulation::setDelay(long delay)
{
    std::lock_guard<std::mutex> lock(this->averageMessageDelayMutex);
    this->averageMessageDelay = delay;
    this->dist.param(boost::random::exponential_distribution<>::param_type(1.0 / this->averageMessageDelay));
}

double Emulation::getDelay()
{
    std::lock_guard<std::mutex> lock(this->averageMessageDelayMutex);
    return this->dist(this->gen);
}
