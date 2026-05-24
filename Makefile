CXX = g++
BUILD_DIR = build
SRC_DIR = src
INCLUDE_DIR = include

CXX_FLAGS = -std=c++23 -O2 -Wall -Wextra
CPPFLAGS = -I$(INCLUDE_DIR) -MMD -MP
LD_FLAGS = -lssl -lcrypto

TARGET = sikradio

SRC_P = $(wildcard $(SRC_DIR)/*.cpp)
SRC_OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_P))
DEPS = $(SRC_OBJ:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC_OBJ)
	$(CXX) $(CXX_FLAGS) -o $@ $^ $(LD_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXX_FLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(SRC_OBJ) $(DEPS) $(TARGET)