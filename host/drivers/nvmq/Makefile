# SPDX-License-Identifier: GPL-2.0-only

# obj-m		+= core.o fabrics.o qdma.o
obj-m		+= nvmq.o

nvmq-objs := core.o fabrics.o qdma.o trace.o
 
PWD := $(CURDIR) 
KSRC = /home/qemu/linux

KBUILD_EXTRA_SYMBOLS := /home/qemu/qdma/driver/src/Module.symvers
#ccflags-y := -DDEBUG
ccflags-y := -UDEBUG 
all: 
	make -C $(KSRC) M=$(PWD) modules 
 
clean: 
	make -C $(KSRC) M=$(PWD) clean
