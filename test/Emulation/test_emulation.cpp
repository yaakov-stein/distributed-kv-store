#include "Emulation.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace Emu;

// シナリオ1: 基本的なメッセージ送受信のテスト
void testBasicMessage() {
    std::cout << "Running Basic Message Test\n";
    Emulation em;
    // 全プロセスを完全に接続
    em.connectNetwork({1}, {2});
    em.connectNetwork({2}, {1});

    // 各プロセスの動作をテストするシンプルな関数
    auto processFunc = [&em](Emulation::PID pid) {
        while (true) {
            auto msg = em.receiveMessage();
            std::cout << "[Process " << pid << "] received: " << msg.second << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    em.spawn(1, [&]() { processFunc(1); });
    em.spawn(2, [&]() { processFunc(2); });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    Emulation::currentPid = 1;
    em.send(2, "Hello from process 1");

    std::this_thread::sleep_for(std::chrono::seconds(3));
}

// シナリオ2: ブロードキャストテスト
void testBroadcast() {
    std::cout << "Running Broadcast Test\n";
    Emulation em;
    // 3つのプロセスを相互接続
    em.connectNetwork({1}, {2});
    em.connectNetwork({1}, {3});
    em.connectNetwork({2}, {3});
    em.connectNetwork({2}, {1});
    em.connectNetwork({3}, {1});
    em.connectNetwork({3}, {2});

    auto processFunc = [&em](Emulation::PID pid) {
        while (true) {
            auto msg = em.receiveMessage();
            std::cout << "[Process " << pid << "] received: " << msg.second << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    em.spawn(1, [&]() { processFunc(1); });
    em.spawn(2, [&]() { processFunc(2); });
    em.spawn(3, [&]() { processFunc(3); });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    Emulation::currentPid = 2;
    em.broadcast("Broadcast message from process 2");

    std::this_thread::sleep_for(std::chrono::seconds(3));
}

// シナリオ3: ネットワークパーティションテスト
void testPartition() {
    std::cout << "Running Network Partition Test\n";
    Emulation em;
    // 2つのプロセスを完全接続
    em.connectNetwork({1}, {2});
    em.connectNetwork({2}, {1});

    auto processFunc = [&em](Emulation::PID pid) {
        while (true) {
            auto msg = em.receiveMessage();
            std::cout << "[Process " << pid << "] received: " << msg.second << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    em.spawn(1, [&]() { processFunc(1); });
    em.spawn(2, [&]() { processFunc(2); });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // パーティションを設定して、プロセス1と2の通信を遮断
    em.partitionNetwork({1}, {2});

    Emulation::currentPid = 1;
    std::cout << "Process 1 sending message while partitioned (should be dropped).\n";
    em.send(2, "Message during partition");

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ネットワークを再接続して再度メッセージ送信
    em.connectNetwork({1}, {2});
    std::cout << "Network reconnected. Process 1 sending message again.\n";
    em.send(2, "Message after reconnection");

    std::this_thread::sleep_for(std::chrono::seconds(3));
}

int main(int argc, char* argv[]) {
    // コマンドライン引数によってシナリオを選択する
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [basic|broadcast|partition]\n";
        return 1;
    }

    std::string scenario = argv[1];
    if (scenario == "basic") {
        testBasicMessage();
    } else if (scenario == "broadcast") {
        testBroadcast();
    } else if (scenario == "partition") {
        testPartition();
    } else {
        std::cout << "Unknown scenario: " << scenario << "\n";
        return 1;
    }

    return 0;
}