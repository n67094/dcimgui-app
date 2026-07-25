#
# Makefile for building: example
#
# Command examples:
#
#   make                              Build the project
#   make DEBUG=1                      Build with debug symbols
#   make SANITIZER=1                  Build with address sanitizer
#   make DEBUG=1 SANITIZER=1          Build with debug symbols and address sanitizer
#   make clean                        Clean build artifacts
#
# Authors: nsix
#

# Config
TARGET := app

BUILD_DIR := build
SRCS_DIR := src

LIB_DIR := lib

IMGUI_DIR := $(LIB_DIR)/dcimgui

# Options
DEBUG ?= 0
SANITIZER ?= 0

# Compiler
CC = gcc
CXX = g++
CFLAGS = -std=c99 -Wall -I$(SRCS_DIR) -I$(LIB_DIR) -I$(IMGUI_DIR)
CXXFLAGS = -std=c++11 -Wall -I$(SRCS_DIR) -I$(LIB_DIR) -I$(IMGUI_DIR)
LDFLAGS = -lSDL3

# Compile with debug
ifeq ($(DEBUG), 1)
CFLAGS += -g -O0 -DDEBUG
CXXFLAGS += -g -O0 -DDEBUG
else
CFLAGS += -O2
CXXFLAGS += -O2
endif

# Conpile with sanitizer
ifeq ($(SANITIZER), 1)
CFLAGS += -fsanitize=address -fno-omit-frame-pointer
CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address
endif

# Source files (recursively search through subdirectories)
SRCS := $(shell find $(SRCS_DIR) -name '*.c')
CXX_SRCS := $(shell find $(SRCS_DIR) -name '*.cpp')

LIB_SRCS := $(shell find $(LIB_DIR) -name '*.c')
LIB_CXX_SRCS := $(shell find $(LIB_DIR) -name '*.cpp')

OBJS := $(patsubst $(SRCS_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS)) \
				$(patsubst $(SRCS_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(CXX_SRCS)) \
				$(patsubst $(LIB_DIR)/%.c, $(BUILD_DIR)/$(LIB_DIR)/%.o, $(LIB_SRCS)) \
				$(patsubst $(LIB_DIR)/%.cpp, $(BUILD_DIR)/$(LIB_DIR)/%.o, $(LIB_CXX_SRCS))

# List of unique directories needed under BUILD_DIR (mirrors SRCS_DIR tree)
OBJ_DIRS := $(sort $(dir $(OBJS)))

.PHONY: all clean
.DEFAULT_GOAL := all

all: $(TARGET)

# Create build directories (including subdirectories)
$(OBJ_DIRS):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRCS_DIR)/%.c | $(OBJ_DIRS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRCS_DIR)/%.cpp | $(OBJ_DIRS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(LIB_DIR)/%.o: $(LIB_DIR)/%.c | $(OBJ_DIRS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(LIB_DIR)/%.o: $(LIB_DIR)/%.cpp | $(OBJ_DIRS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) core.*
