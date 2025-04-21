# compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -I./src/Emulation -I./src/Paxos -Wall -Wextra
LDFLAGS = -pthread

# source and test 
SRC_EMULATION = src/Emulation/Emulation.cpp
SRC_PAXOS = src/Paxos/Paxos.cpp
# TEST_1_SRC = test/Emulation/test_emulation.cpp
# TEST_SRC = test/Paxos/test_paxos_cmd.cpp
TEST_SRC = test/Paxos/test_paxos.cpp

# output file name
TARGET = test_paxos

# target
all: $(TARGET)

# build rule for each target
# $(TARGET): $(SRC) $(TEST_SRC)
# 	$(CXX) $(CXXFLAGS) $(SRC) $(TEST_SRC) $(LDFLAGS) -o $(TARGET)
$(TARGET): $(SRC_EMULATION) $(SRC_PAXOS) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(SRC_EMULATION) $(SRC_PAXOS) $(TEST_SRC) $(LDFLAGS) -o $(TARGET)

# test exec
test: $(TARGET)
	./$(TARGET) $(ARGS)

# cleanup
clean:
	rm -f $(TARGET)

.PHONY: all test clean