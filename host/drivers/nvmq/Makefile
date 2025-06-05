# SPDX-License-Identifier: GPL-2.0-only

# obj-m		+= core.o fabrics.o qdma.o
obj-m		+= nvmq.o

nvmq-objs := core.o fabrics.o qdma.o trace.o
 
PWD := $(CURDIR)
KSRC = $(PWD)/../../kernel/source

KBUILD_EXTRA_SYMBOLS += "$(PWD)/../qdma/driver/src/Module.symvers"
#KBUILD_EXTRA_SYMBOLS += /mnt/suda/host/drivers/qdma/driver/src/Module.symvers
export KBUILD_EXTRA_SYMBOLS
#ccflags-y := -DDEBUG
ccflags-y := -UDEBUG -DTRACE_INCLUDE_PATH=\"$(PWD)\"
all: 
	@echo "symbols ${KBUILD_EXTRA_SYMBOLS}"
	make -C $(KSRC) M=$(PWD) modules 
 
clean: 
	make -C $(KSRC) M=$(PWD) clean
