ROOT_DIR := $(abspath $(CURDIR)/../..)
REMOTE_PROTOCOL_DIR := ../vscode-lwe-encrypt-remote-offload
include $(ROOT_DIR)/mk/common.mk

APP = vscode-selective-lwe-full-pipeline
OBJS = vscode-selective-lwe-full-pipeline.o sql_filter_compiler.o lwe_remote_protocol.o

CXXFLAGS += -I$(ROOT_DIR)/api/libnvme/src
CXXFLAGS += -I$(REMOTE_PROTOCOL_DIR)
CXXFLAGS += -I../vscode-selective-lwe-encrypt-offload
CXXFLAGS += -I$(ROOT_DIR)/../device/operators/hls/selective_filter

LIBS = $(ROOT_DIR)/api/libnvme/.build/src/libnvme.a
LIBS += -lssl -lcrypto -lstdc++ -lm -lpthread

.PHONY: all clean test

all: $(APP)

$(APP): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

lwe_remote_protocol.o: $(REMOTE_PROTOCOL_DIR)/lwe_remote_protocol.cpp \
	$(REMOTE_PROTOCOL_DIR)/lwe_remote_protocol.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

sql_filter_compiler.o: ../vscode-selective-lwe-encrypt-offload/sql_filter_compiler.cpp \
	../vscode-selective-lwe-encrypt-offload/sql_filter_compiler.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(APP)
	./$(APP) --self-test

clean:
	rm -f $(APP) $(OBJS)
