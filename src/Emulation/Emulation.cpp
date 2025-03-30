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