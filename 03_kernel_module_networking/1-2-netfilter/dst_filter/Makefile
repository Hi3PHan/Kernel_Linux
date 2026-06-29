ifneq ($(KERNELRELEASE),)

obj-m += dst_filter.o
dst_filter-y := kernel/dst_filter_main.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
USER_BIN := filterctl

.PHONY: all module user clean

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	$(CC) -Wall -Wextra -O2 -Iinclude -o $(USER_BIN) user/filterctl.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_BIN)

endif
