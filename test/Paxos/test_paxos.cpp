#include "Emulation.hpp"
#include "Paxos.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

using namespace Emu;
using namespace Paxos;

#define RUN_TEST(fn)                             \
  do {                                           \
    std::cout << #fn << " ... ";               \
    bool ok = fn();                              \
    std::cout << (ok ? "PASS" : "FAIL") << "\n"; \
    if (ok) ++__tests_passed; else ++__tests_failed; \
  } while (0)

#define DECLARE_TEST_COUNTERS \
static int __tests_passed = 0, __tests_failed = 0;

/**
 * @brief Test normal operation of MultiPaxos.
 *
 * This test case simulates a normal operation of MultiPaxos by spawning 3 nodes,
 * connecting them in a full mesh, and then sending a client request to the leader
 * node. The test verifies that the request was successfully executed and the state
 * of all nodes is consistent.
 */
  bool testMultiPaxosNormalOperation() {
    std::cout << "Running MultiPaxos Normal Operation Test\n";
    Emulation* em = new Emulation();
    Configuration cfg({1, 2, 3});
    em->setAverageMessageDelay(0);

    // Connect full mesh
    em->connectNetwork({1}, {2, 3});
    em->connectNetwork({2}, {1, 3});
    em->connectNetwork({3}, {1, 2});
    std::cout << "Connected network\n";

    // Create storage for node pointers
    std::vector<MultiPaxos*> nodes(4, nullptr);

    // Spawn node 1 as leader
    em->spawn(1, [&]() {
        Emulation::currentPid = 1;
        nodes[1] = new MultiPaxos(1, cfg, em);
        nodes[1]->becomeLeader();  // force leader
        nodes[1]->run();
    });

    // Spawn node 2 and 3 as members
    for (int i : {2, 3}) {
        em->spawn(i, [&, i]() {
            Emulation::currentPid = i;
            nodes[i] = new MultiPaxos(i, cfg, em);
            nodes[i]->becomeMember(1);  // member under P1
            nodes[i]->run();
        });
    }

    std::cout << "Spawned nodes\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Simulate client request to leader (PID=1)
    int leaderId = 1;
    Emulation::currentPid = 99;  // client PID
    // em->connectNetwork({99}, {leaderId});
    Message req{MessageType::CLIENT, 0, {}, Command{Command::OpType::ADD, "x", 42}, 99, 0};
    std::cout << "[Client] Sending ADD(x=42) to P" << leaderId << "\n";
    em->send(leaderId, serializeMessage(req));

    // Allow time for consensus
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Verify key-value stores
    bool success = true;
    for (int i = 1; i <= 3; ++i) {
        auto kv = nodes[i]->getState();
        auto it = kv.find("x");
        bool ok = (it != kv.end() && it->second == 42);
        std::cout << "[P" << i << "] store[\"x\"] = "
                  << (it != kv.end() ? std::to_string(it->second) : "<none>")
                  << " => " << (ok ? "OK" : "FAIL") << "\n";
        success &= ok;
    }
    return success;
}


/**
 * @brief Test MultiPaxos with message delay.
 *
 * This test spawns a 3-node network and waits for the leader election to
 * complete.  It then sends a single ADD(x=42) command to the leader, which
 * should be replicated to all nodes after consensus is reached.  The network
 * delay is set to 10ms to ensure that messages are properly serialized and
 * the consensus protocol works correctly.
 */
bool testMultiPaxosDelayOperation() {
    std::cout << "Running MultiPaxos Delay Operation Test\n";
    Emulation* em = new Emulation();
    Configuration cfg({1, 2, 3});
    em->setAverageMessageDelay(10);

    // Connect full mesh
    em->connectNetwork({1}, {2, 3});
    em->connectNetwork({2}, {1, 3});
    em->connectNetwork({3}, {1, 2});
    std::cout << "Connected network\n";

    // Create storage for node pointers
    std::vector<MultiPaxos*> nodes(4, nullptr);

    // Spawn node 1 as leader
    em->spawn(1, [&]() {
        Emulation::currentPid = 1;
        nodes[1] = new MultiPaxos(1, cfg, em);
        nodes[1]->becomeLeader();  // force leader
        nodes[1]->run();
    });

    // Spawn node 2 and 3 as members
    for (int i : {2, 3}) {
        em->spawn(i, [&, i]() {
            Emulation::currentPid = i;
            nodes[i] = new MultiPaxos(i, cfg, em);
            nodes[i]->becomeMember(1);  // member under P1
            nodes[i]->run();
        });
    }

    std::cout << "Spawned nodes\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Simulate client request to leader (PID=1)
    int leaderId = 1;
    Emulation::currentPid = 99;  // client PID
    // em->connectNetwork({99}, {leaderId});
    Message req{MessageType::CLIENT, 0, {}, Command{Command::OpType::ADD, "x", 42}, 99, 0};
    std::cout << "[Client] Sending ADD(x=42) to P" << leaderId << "\n";
    em->send(leaderId, serializeMessage(req));

    // Allow time for consensus
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Verify key-value stores
    bool success = true;
    for (int i = 1; i <= 3; ++i) {
        auto kv = nodes[i]->getState();
        auto it = kv.find("x");
        bool ok = (it != kv.end() && it->second == 42);
        std::cout << "[P" << i << "] store[\"x\"] = "
                  << (it != kv.end() ? std::to_string(it->second) : "<none>")
                  << " => " << (ok ? "OK" : "FAIL") << "\n";
        success &= ok;
    }
    return success;
}

// 3) Manual failover & commit
bool testManualFailoverAndCommit() {
    Emulation* em = new Emulation();
    Configuration cfg({1,2,3});
    em->setAverageMessageDelay(0);
    em->connectNetwork({1},{2,3}); em->connectNetwork({2},{1,3}); em->connectNetwork({3},{1,2});
    std::vector<MultiPaxos*> nodes(4,nullptr);
    for(int i=1;i<=3;i++) em->spawn(i,[&,i]{ Emulation::currentPid=i; nodes[i]=new MultiPaxos(i,cfg,em); nodes[i]->run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Step 1: P1 leader
    nodes[1]->becomeLeader(); nodes[2]->becomeMember(1); nodes[3]->becomeMember(1);
    Emulation::currentPid=99;
    Message r1{MessageType::CLIENT,0,{},Command{Command::OpType::ADD,"alpha",100},99,0};
    em->send(1,serializeMessage(r1));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for(int i=1;i<=3;i++){
        auto st=nodes[i]->getState();
        if(!(st.count("alpha")&&st["alpha"]==100)) return false;
    }
    // Step 2: kill P1, promote P2
    em->killProcess(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    nodes[2]->becomeLeader(); nodes[3]->becomeMember(2);
    Emulation::currentPid=99;
    Message r2{MessageType::CLIENT,1,{},Command{Command::OpType::ADD,"beta",200},99,0};
    em->send(2,serializeMessage(r2));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for(int i=2;i<=3;i++){
        auto st=nodes[i]->getState();
        if(!(st.count("alpha")&&st["alpha"]==100)) return false;
        if(!(st.count("beta")&&st["beta"]==200)) return false;
    }
    return true;
}

/**
 * @brief Test manual failover and commit with network delay.
 *
 * This test case sends two commands: first to the leader, then kill the leader, and
 * then send the second command to the new leader. The test verifies that both commands
 * were committed and the state is consistent across all nodes.
 *
 * The network delay is set to 10ms, which is enough to ensure that messages are
 * properly serialized and the consensus protocol works correctly.
 */
bool testManualFailoverAndCommitAndDelay() {
    Emulation* em = new Emulation();
    Configuration cfg({1,2,3});
    em->setAverageMessageDelay(10);
    em->connectNetwork({1},{2,3}); em->connectNetwork({2},{1,3}); em->connectNetwork({3},{1,2});
    std::vector<MultiPaxos*> nodes(4,nullptr);
    for(int i=1;i<=3;i++) em->spawn(i,[&,i]{ Emulation::currentPid=i; nodes[i]=new MultiPaxos(i,cfg,em); nodes[i]->run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Step 1: P1 leader
    nodes[1]->becomeLeader(); nodes[2]->becomeMember(1); nodes[3]->becomeMember(1);
    Emulation::currentPid=99;
    Message r1{MessageType::CLIENT,0,{},Command{Command::OpType::ADD,"alpha",100},99,0};
    em->send(1,serializeMessage(r1));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for(int i=1;i<=3;i++){
        auto st=nodes[i]->getState();
        if(!(st.count("alpha")&&st["alpha"]==100)) return false;
    }
    // Step 2: kill P1, promote P2
    em->killProcess(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    nodes[2]->becomeLeader(); nodes[3]->becomeMember(2);
    Emulation::currentPid=99;
    Message r2{MessageType::CLIENT,1,{},Command{Command::OpType::ADD,"beta",200},99,0};
    em->send(2,serializeMessage(r2));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for(int i=2;i<=3;i++){
        auto st=nodes[i]->getState();
        if(!(st.count("alpha")&&st["alpha"]==100)) return false;
        if(!(st.count("beta")&&st["beta"]==200)) return false;
    }
    return true;
}

/**
 * @brief Test execution of multiple commands in MultiPaxos.
 *
 * This test initializes a MultiPaxos instance with three nodes, where one is
 * a leader and the others are members. It simulates multiple client commands
 * being sent to the leader for execution. The commands include adding and
 * updating key-value pairs in the state machine. The test checks whether all
 * commands are correctly committed and replicated across nodes, verifying the
 * final state of one of the member nodes.
 *
 * @return True if the final state of the member node matches the expected
 *         values after applying all commands, false otherwise.
 */

bool testMultipleCommands() {
    std::cout << "Running Multiple Commands Test\n";
    Emulation* em=new Emulation();
    Configuration cfg({1,2,3});
    em->setAverageMessageDelay(0);
    em->connectNetwork({1},{2,3}); em->connectNetwork({2},{1,3}); em->connectNetwork({3},{1,2});

    std::vector<MultiPaxos*> nodes(4);
    em->spawn(1,[&]{ Emulation::currentPid=1; nodes[1]=new MultiPaxos(1,cfg,em); nodes[1]->becomeLeader(); nodes[1]->run(); });
    for(int i:{2,3}) em->spawn(i,[&,i]{ Emulation::currentPid=i; nodes[i]=new MultiPaxos(i,cfg,em); nodes[i]->becomeMember(1); nodes[i]->run(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    Emulation::currentPid=99;
    std::vector<Command> cmds={{Command::OpType::ADD,"a",1},{Command::OpType::ADD,"b",2},{Command::OpType::UPDATE,"a",3}};
    int i=0;
    for(auto &c:cmds) {
        Message req{MessageType::CLIENT,i,{},c,99,0};
        em->send(1, serializeMessage(req));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        i++;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto state=nodes[2]->getState();
    return state["a"]==3 && state["b"]==2;
}

int main() {
    DECLARE_TEST_COUNTERS;
    RUN_TEST(testMultiPaxosNormalOperation);
    RUN_TEST(testMultiPaxosDelayOperation);
    RUN_TEST(testManualFailoverAndCommitAndDelay);
    RUN_TEST(testManualFailoverAndCommit);
    RUN_TEST(testMultipleCommands);
    std::cout << "\n=== TEST SUMMARY ===\n"
              << "  Passed: " << __tests_passed << "\n"
              << "  Failed: " << __tests_failed << "\n";
    return __tests_failed == 0 ? 0 : 1;
}
