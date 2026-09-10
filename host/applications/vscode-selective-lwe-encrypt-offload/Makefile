ROOT_DIR := $(abspath $(CURDIR)/../..)
include $(ROOT_DIR)/mk/common.mk

APP = vscode-selective-lwe-encrypt-offload
CXX_SRCS := vscode-selective-lwe-encrypt-offload.cpp sql_filter_compiler.cpp

CXXFLAGS += -I$(ROOT_DIR)/api/libnvme/src
CXXFLAGS += -I$(ROOT_DIR)/../device/operators/hls/selective_filter

OBJS = $(CXX_SRCS:.cpp=.o)
LIBS = $(ROOT_DIR)/api/libnvme/.build/src/libnvme.a
LIBS += -lssl -lcrypto -lstdc++ -lm -lpthread

.PHONY: all clean test

all: $(APP)

$(APP): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(APP) $(OBJS) test_sql_filter_compiler

test: test_sql_filter_compiler
	./test_sql_filter_compiler

test_sql_filter_compiler: test_sql_filter_compiler.cpp sql_filter_compiler.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
