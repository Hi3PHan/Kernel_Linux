ifneq ($(KERNELRELEASE),)

obj-m += tcp_udp_steg.o
tcp_udp_steg-y := kernel/tcp_udp_steg_main.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
USER_BIN := stegctl

.PHONY: all module user clean

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	$(CC) -Wall -Wextra -O2 -Iinclude -o $(USER_BIN) user/stegctl.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_BIN)

endif
