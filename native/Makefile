# Linux build. Windows builds with CMake (see CMakeLists.txt).
CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
CORE := $(wildcard src/core/*.cpp) $(wildcard src/app/*.cpp) $(wildcard src/render/*.cpp)
LINUX := $(wildcard src/platform/linux/*.cpp)
BUILD := build

all: $(BUILD)/gato-de-mesa $(BUILD)/snapshot $(BUILD)/bench

$(BUILD)/gato-de-mesa: $(CORE:%.cpp=$(BUILD)/%.o) $(LINUX:%.cpp=$(BUILD)/%.o)
	$(CXX) $^ -o $@ -lX11 -lXext

$(BUILD)/snapshot: $(CORE:%.cpp=$(BUILD)/%.o) $(BUILD)/src/tools/snapshot.o
	$(CXX) $^ -o $@

$(BUILD)/src/tools/snapshot.o: src/tools/snapshot.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DASSET_DIR='"$(CURDIR)/assets"' -MMD -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
.PHONY: all clean

$(BUILD)/bench: $(CORE:%.cpp=$(BUILD)/%.o) $(BUILD)/src/tools/bench.o
	$(CXX) $^ -o $@

$(BUILD)/src/tools/bench.o: src/tools/bench.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DASSET_DIR='"$(CURDIR)/assets"' -MMD -c $< -o $@
