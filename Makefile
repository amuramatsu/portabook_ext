EXTRA_CFLAGS += $(USER_EXTRA_CFLAGS)
EXTRA_CFLAGS += -O2
EXTRA_CFLAGS += -Wall

########################## Features ###########################
CONFIG_PORTABOOK_EXT_BACKLIGHT = y
CONFIG_PORTABOOK_EXT_BATTERY = y
###############################################################

KVER ?= $(shell uname -r)
KERNEL_DIR = /lib/modules/$(KVER)/build
BUILD_DIR := $(shell pwd)
VERBOSE   := 0
MODULE_NAME = portabook_ext
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/platform/x86/

$(MODULE_NAME)-y := portabook_init.o
$(MODULE_NAME)-$(CONFIG_PORTABOOK_EXT_BACKLIGHT) += portabook_backlight.o
$(MODULE_NAME)-$(CONFIG_PORTABOOK_EXT_BATTERY) += portabook_battery.o
obj-m      := portabook_ext.o

ifeq ($(CONFIG_PORTABOOK_EXT_BACKLIGHT), y)
EXTRA_CFLAGS += -DCONFIG_PORTABOOK_EXT_BACKLIGHT
endif

ifeq ($(CONFIG_PORTABOOK_EXT_BATTERY), y)
EXTRA_CFLAGS += -DCONFIG_PORTABOOK_EXT_BATTERY
endif

all:
	make -C $(KERNEL_DIR) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=$(VERBOSE) modules

strip:
	strip $(MODULE_NAME).ko --strip-unneeded

install:
	install -p -m 644 $(MODULE_NAME).ko $(MODDESTDIR)

uninstall:
	rm -r $(MODDESTDIR)/$(MODULE_NAME).ko

.PHONY: clean clobber

clean:
	rm -f  *.o *.ko *.mod.c *.symvers *.order .portabook*
	rm -fr .tmp_versions

clobber: clean
	rm -f *~ *.bak
