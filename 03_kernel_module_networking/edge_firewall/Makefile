ifneq ($(KERNELRELEASE),)

obj-m += local_fw.o
obj-m += forward_fw.o

local_fw-y := kernel/local_fw_main.o
forward_fw-y := kernel/forward_fw_main.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
USER_BIN := fwctl

.PHONY: all module user scripts clean

all: module user scripts

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	$(CC) -Wall -Wextra -O2 -Iinclude -o $(USER_BIN) user/fwctl.c

scripts:
	chmod +x scripts/*.sh test_edge_firewall.sh

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_BIN)

endif
