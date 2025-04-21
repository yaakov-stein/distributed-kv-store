// test_paxos_unit.cpp

#include "Paxos.hpp"
#include <iostream>
#include <cassert>
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

// Test1: explicit leader election via onElectionMessage
bool Test1_LeaderElection() {
  Emulation* em = new Emulation();
  Configuration cfg({1,2,3});
  MultiPaxos n1(1, cfg, em);

  // Candidate declares itself
  n1.becomeCandidate();

  // Two votes arrive
  Message vote1{ MessageType::VOTE, 0, {0,0}, Command(), /*sender*/2, /*candidate*/1 };
  Message vote2{ MessageType::VOTE, 0, {0,0}, Command(), /*sender*/3, /*candidate*/1 };
  n1.onElectionMessage(vote1);
  n1.onElectionMessage(vote2);

  return n1.getLeaderState() == LeaderState::LEADER;
}

// Test2: apply a decided command via onDecide
bool Test2_KVCommands() {
  Configuration cfg({1,2,3});
  Emulation* em = new Emulation();
  MultiPaxos n1(1, cfg, em), n2(2, cfg, em), n3(3, cfg, em);

  // Pretend 1 is already leader
  n1.becomeLeader();
  n2.becomeMember(1);
  n3.becomeMember(1);

  // A DECIDE message for slot 0, ADD x=10
  Message dec{
    MessageType::DECIDE,
    /*slot*/0,
    /*ballot*/{0,1},
    /*cmd*/Command{Command::OpType::ADD, "x", 10},
    /*sender*/1,
    /*candidate*/1
  };

  // All three nodes apply it
  n1.onDecide(dec);
  n2.onDecide(dec);
  n3.onDecide(dec);

  auto s1 = n1.getState();
  auto s2 = n2.getState();
  auto s3 = n3.getState();
  return s1["x"]==10 && s2["x"]==10 && s3["x"]==10;
}

// Test3: failover purely via onElectionMessage
bool Test3_Failover() {
  Configuration cfg({1,2,3});
  Emulation* em = new Emulation();
  MultiPaxos n2(2, cfg, em);

  // Node 2 times out and becomes candidate
  n2.becomeCandidate();

  // Two votes turn up
  Message v1{ MessageType::VOTE,0,{0,0},Command(), /*sender*/1, /*candidate*/2 };
  Message v2{ MessageType::VOTE,0,{0,0},Command(), /*sender*/3, /*candidate*/2 };
  n2.onElectionMessage(v1);
  n2.onElectionMessage(v2);

  return n2.getLeaderState() == LeaderState::LEADER;
}

int main(){
  DECLARE_TEST_COUNTERS;
  RUN_TEST(Test1_LeaderElection);
  RUN_TEST(Test2_KVCommands);
  RUN_TEST(Test3_Failover);
  std::cout << "\n=== TEST SUMMARY ===\n"
              << "  Passed: " << __tests_passed << "\n"
              << "  Failed: " << __tests_failed << "\n";
  return __tests_failed == 0 ? 0 : 1;
}