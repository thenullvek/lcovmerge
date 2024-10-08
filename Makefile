# Makefile for lcovmerge

config ?= release
BUILD=build/$(config)

CXXFLAGS+= -std=c++17 -Wall -Werror
CFLAGS+= -Wall -Werror
ifeq ($(config),release)
	CXXFLAGS+= -O3 -DNDEBUG -fno-rtti
	CFLAGS+= -O3
endif
ifeq ($(config),debug)
	CXXFLAGS+= -g -O0
	CFLAGS+= -g
endif

GTEST_FLAGS:= $(shell pkg-config --cflags gtest_main)
GTEST_LDFLAGS=$(shell pkg-config --libs gtest_main)
ifeq ($(config),test)
CFLAGS+= -O0 $(GTEST_FLAGS)
CXXFLAGS+= -O0 $(GTEST_FLAGS) -DTESTMODE 
LDFLAGS+= $(GTEST_LDFLAGS)
endif

LM_SRCS=$(wildcard src/*.c src/*.cc)
LM_OBJS=$(LM_SRCS:%=$(BUILD)/%.o)
LM_TARGET=$(BUILD)/lcovmerge
LM_TEST_SRCS=$(filter-out src/getopt.c,$(LM_SRCS)) $(wildcard tests/*.cc)
LM_TEST_OBJS=$(LM_TEST_SRCS:%=$(BUILD)/%.o)
LM_TEST_TARGET=$(BUILD)/run_tests

.PHONY: all clean compdb
all:
clean:
	rm -rf $(BUILD)

compdb:
	-rm -f compile_commands.json
	$(MAKE) config=test clean
	bear -- $(MAKE) config=test

ifneq ($(config),test)

.PHONY: lcovmerge
all: lcovmerge
lcovmerge: $(LM_TARGET)

$(LM_TARGET): $(LM_OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

else

all: run_tests
.PHONY: run_tests

run_tests: $(LM_TEST_TARGET)

$(LM_TEST_TARGET): $(LM_TEST_OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

endif

$(BUILD)/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $< $(CXXFLAGS) -c -MMD -MP -o $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $< $(CFLAGS) -c -MMD -MP -o $@

-include $(LM_OBJS:.o=.d) $(LM_TEST_OBJS:.o=.d)

