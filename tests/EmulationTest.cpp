#include <gtest/gtest.h>
#include <thread>
#include <unordered_set>
#include <functional>
#include <memory>
#include <chrono>
#include "../src/Emulation/Emulation.hpp"
#include "../src/Message/Message.hpp"
#include "../src/Message/NoopMessage.hpp"

// Test the Emulation class
class EmulationTest : public ::testing::Test {
protected:
    std::shared_ptr<Emulation> emulation;

    void SetUp() override {
        emulation = std::make_shared<Emulation>();
        emulation->setDelay(1);
    }

    // A utility to create a simple Message
    std::unique_ptr<const Message> createMessage() {
         std::cout << "[checkMessage] Current thread (" << std::this_thread::get_id() << ")\n";
        return std::make_unique<NoopMessage>(std::this_thread::get_id());
    }

    // A utility to spawn a dummy process
    std::thread::id spawnDummyProcess() {
        return emulation->spawn([] {
            std::this_thread::sleep_for(std::chrono::seconds(1));  // Simulate some work
        });
    }

    std::thread::id spawnListenerProcess() {
        return emulation->spawn([emu=this->emulation] {
            bool received = false;
            while(!received)
            {
                std::unique_ptr<const Message> msg = emu->receiveMessage();
                if(msg != nullptr)
                    received = true;
            }
        });
    }

    // std::thread::id spawnResponderProcess() {
    //     return emulation->spawn([&emu=this->emulation] {
    //         bool received = false;
    //         while(!received)
    //         {
    //             std::unique_ptr<const Message> msg = emu->receiveMessage();
    //             if(msg != nullptr)
    //             {
    //                 emu->send(msg->getSender(), std::make_unique<const NoopMessage>(std::this_thread::get_id()));
    //                 received = true;
    //             }
    //         }
    //     });
    // }
    std::thread::id spawnResponderProcess() {
    return emulation->spawn([emu = this->emulation] {
        bool received = false;
        std::thread::id myThreadId = std::this_thread::get_id();
        std::cout << "[spawnResponderProcess] Thread started. Thread ID: " << myThreadId << std::endl;

        while (!received) {
            std::cout << "[spawnResponderProcess] Waiting to receive a message..." << std::endl;
            std::unique_ptr<const Message> msg = emu->receiveMessage();

            if (msg != nullptr) {
                std::cout << "[spawnResponderProcess] Received a message from sender Thread ID: "
                          << msg->getSender() << std::endl;

                bool sendResult = emu->send(msg->getSender(),
                    std::make_unique<const NoopMessage>(myThreadId));
                
                std::cout << "[spawnResponderProcess] Sent NoopMessage back to sender. Success: "
                          << (sendResult ? "true" : "false") << std::endl;

                received = true;
            } else {
                std::cout << "[spawnResponderProcess] Received a null message. Retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(7));
            }
        }

        std::cout << "[spawnResponderProcess] Exiting responder loop." << std::endl;
    });
}
};

// Test for spawning a process
TEST_F(EmulationTest, TestSpawn) {
    // Check if spawn works without any crashes
    EXPECT_NO_THROW(spawnDummyProcess());
}

// Test for successfully sending a message
TEST_F(EmulationTest, TestSendMessageSuccess) {
    std::thread::id receiverPid = spawnListenerProcess();;

    // Send a message to the receiver
    bool result = emulation->send(receiverPid, std::move(createMessage()));

    // The send should return true as we assume the receiver is reachable
    EXPECT_TRUE(result);
}

// Test for sending a message to a process that cannot be reached
TEST_F(EmulationTest, TestSendMessageFailure) {
    emulation->setDelay(1);
    // Spawn only one process (the sender), no receiver
    spawnDummyProcess();

    std::thread::id receiverPid;  // Invalid receiver
    auto message = createMessage();

    // Send should fail as the receiver is not available
    bool result = emulation->send(receiverPid, std::move(message));

    EXPECT_FALSE(result);
}

// Test for receiving a message
// TEST_F(EmulationTest, TestReceiveMessage) {
//     emulation.init();
//     // Spawn a process and send a message to it
//     std::thread::id receiverPid = spawnListenerProcess();
//     auto message = createMessage();
//     emulation->send(receiverPid, std::move(message));

//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     // Receive the message
//     auto receivedMessage = emulation.receiveMessage();

//     // Ensure the message was received and is not null
//     EXPECT_NE(receivedMessage, nullptr);
// }

TEST_F(EmulationTest, TestReceiveMessage) {
    std::cout << "[Test] Initializing emulation..." << std::endl;
    emulation->setDelay(1);

    std::cout << "[Test] Spawning listener process..." << std::endl;
    std::thread::id receiverPid = spawnResponderProcess();
    std::cout << "[Test] Responder process spawned with PID: " << receiverPid << std::endl;

    std::cout << "[Test] Creating message to send..." << std::endl;
    auto message = createMessage();
    std::cout << "[Test] Sending message to receiver..." << std::endl;
    bool sendResult = emulation->send(receiverPid, std::move(message));

    if (sendResult) {
        std::cout << "[Test] Send succeeded." << std::endl;
    } else {
        std::cout << "[Test] Send failed!" << std::endl;
    }

    std::cout << "[Test] Sleeping to allow message delivery..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[Test] Woke up from sleep." << std::endl;

    std::cout << "[Test] Attempting to receive message..." << std::endl;
    auto receivedMessage = emulation->receiveMessage();

    if (receivedMessage) {
        std::cout << "[Test] Message received successfully!" << std::endl;
    } else {
        std::cout << "[Test] Failed to receive message (nullptr)." << std::endl;
    }

    EXPECT_NE(receivedMessage, nullptr);
}

// Test for partitioning the network
TEST_F(EmulationTest, TestPartitionNetwork) {
    emulation->setDelay(1);

    std::unordered_set<std::thread::id> groupA, groupB;
    std::thread::id tid1 = spawnDummyProcess();
    groupA.insert(tid1);

    std::thread::id tid2 = spawnDummyProcess();
    groupB.insert(tid2);

    // Partition the network
    bool result = emulation->partitionNetwork(groupA, groupB);

    EXPECT_TRUE(result);
}

// Test for reconnecting the network
TEST_F(EmulationTest, TestReconnectNetwork) {
    emulation->setDelay(1);

    std::unordered_set<std::thread::id> groupA, groupB;
    std::thread::id tid1 = spawnDummyProcess();
    groupA.insert(tid1);

    std::thread::id tid2 = spawnDummyProcess();
    groupB.insert(tid2);

    // Partition the network
    emulation->partitionNetwork(groupA, groupB);

    // Reconnect the network
    bool result = emulation->connectNetwork(groupA, groupB);

    EXPECT_TRUE(result);
}

// Test for marking a process as dead
TEST_F(EmulationTest, TestSetIsProcessDead) {
    emulation->setDelay(1);

    spawnDummyProcess();
    std::thread::id pid = std::this_thread::get_id();

    // Mark the process as dead
    emulation->setIsProcessDead(pid, true);

    // Check if the process is dead
    EXPECT_TRUE(emulation->isProcessDead(pid));
}

// Test for setting the message delay
// TEST_F(EmulationTest, TestSetDelay) {
//     long newDelay = 2000;

//     // Set the delay
//     emulation.setDelay(newDelay);

//     // Get the delay and check if it was set correctly
//     double delay = emulation.getDelay();
//     EXPECT_DOUBLE_EQ(delay, 1.0 / newDelay);
// }

// Test for receiving a message when there are no messages
TEST_F(EmulationTest, TestReceiveMessageNoMessages) {

    spawnDummyProcess();

    // Try to receive a message when no messages are sent
    auto receivedMessage = emulation->receiveMessage();

    EXPECT_EQ(receivedMessage, nullptr);  // Should return null
}
