ifneq ($(KERNELRELEASE),)

obj-m += packet_capture.o
packet_capture-y := kernel/packet_capture_main.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
USER_BIN := capturectl

.PHONY: all module user clean

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	$(CC) -Wall -Wextra -O2 -Iinclude -o $(USER_BIN) user/capturectl.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_BIN)

endif
