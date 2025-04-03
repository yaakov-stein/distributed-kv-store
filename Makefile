# コンパイラとオプションの設定
CXX = g++
CXXFLAGS = -std=c++11 -I./src/Emulation -Wall -Wextra
LDFLAGS = -pthread

# ソースファイルとテストファイルの指定
SRC = src/Emulation/Emulation.cpp
TEST_SRC = test/Emulation/test_emulation.cpp

# 出力ファイルの名前
TARGET = test_emulation

# デフォルトターゲット: ビルド
all: $(TARGET)

# テスト用実行ファイルのビルドルール
$(TARGET): $(SRC) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(TEST_SRC) $(LDFLAGS) -o $(TARGET)

# テスト実行ターゲット：引数を渡す場合は ARGS で指定
test: $(TARGET)
	./$(TARGET) $(ARGS)

# クリーンアップ
clean:
	rm -f $(TARGET)

.PHONY: all test clean