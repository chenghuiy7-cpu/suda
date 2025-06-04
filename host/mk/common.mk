# 通用编译参数和规则

CXX ?= g++
CXXFLAGS += -Wall -Wextra -O2 -g -std=c++14
LDFLAGS +=

# 项目目录
API_DIR := $(ROOT_DIR)/api
LIBNVME_DIR := $(API_DIR)/libnvme
LIBURING_DIR := $(API_DIR)/liburing
APPLICATIONS_DIR := $(ROOT_DIR)/applications

# 通用编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.cxx
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $< -o $@
	