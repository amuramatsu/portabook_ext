KVER ?= $(shell uname -r)
KERNEL_DIR = /lib/modules/$(KVER)/build
BUILD_DIR := $(shell pwd)
VERBOSE   := 0
MODULE_NAME = portabook_ext

$(MODULE_NAME)-y := portabook_init.o portabook_backlight.o portabook_battery.o
obj-m      := portabook_ext.o

all:
	make -C $(KERNEL_DIR) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=$(VERBOSE) modules

clean:
	rm -f  *.o *.ko *.mod.c *.symvers *.order .portabook*
	rm -fr .tmp_versions
clobber: clean
	rm -f *~ *.bak
