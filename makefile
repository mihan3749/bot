CXX = clang++
CXXFLAGS = -std=c++17 -Iinclude/ -g
CXXLIBS = -lTgBot -lboost_system -lssl -lcrypto -lpthread -lpcre

SRC_DIR = src
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = tg_bot

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)/*.o $(TARGET)