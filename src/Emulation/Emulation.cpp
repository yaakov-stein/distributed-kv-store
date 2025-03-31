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
                // 各プロセスごとにメッセージキューを作成
                messageQueueMap[pid] = std::make_shared<ProcessQueue>();
                // 初期状態では、各プロセスの接続先リストは空。
                // 必要に応じて connectNetwork() で接続を設定してください。
                reachableMap[pid] = {};
                isDead[pid] = false;
            }
            // プロセスは独立したスレッドで実行する
            std::thread t([this, pid, fn]() {
                currentPid = pid;  // スレッドローカルのプロセスIDを設定
                fn();              // 指定された処理を実行
            });
            t.detach();  // スレッドはデタッチ（join() はしない）
        }
    
        // whoami()
        // 現在のプロセス（スレッド）のIDを返す
        PID whoami() {
            return currentPid;
        }

        void send(PID receiverPid, const Message& message) {
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
            }
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
            // reachableMap[sender] に receiver が存在する場合のみ送信可能
            return (reachableMap[sender].find(receiver) != reachableMap[sender].end());
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
    
        // average message delay
        int averageMessageDelay;
        
        // current process id
        static thread_local PID currentPid;

        bool isProcessDead(PID pid) {
            std::lock_guard<std::mutex> lock(globalMutex);
            return isDead[pid];
        }
        // random delay between 50% to 150% of the specified delay time
        int randomDelay(int delay) {
            static thread_local std::mt19937 generator(std::random_device{}());
            std::uniform_int_distribution<int> distribution(delay / 2, (delay * 3) / 2);
            return distribution(generator);
        }

        // 指定した確率に基づき、メッセージをドロップするかを判定する
        bool shouldDrop(double probability) {
            static thread_local std::mt19937 generator(std::random_device{}());
            std::uniform_real_distribution<double> distribution(0.0, 1.0);
            return distribution(generator) < probability;
        }
};


thread_local Emulation::PID Emulation::currentPid = -1;
