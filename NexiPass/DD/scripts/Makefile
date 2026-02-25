KDIR := /home/brunoa/buildroot/buildroot-2025.02.6/output/build/linux-custom
ARCH ?= arm64
CROSS_COMPILE ?= /home/brunoa/buildroot/buildroot-2025.02.6/output/host/bin/aarch64-buildroot-linux-gnu-
export ARCH CROSS_COMPILE
KBUILD_TARGETS := clean help modules modules_install

obj-m := ledrgb.o
ledrgb-objs:= led_dd.o utility.o

.PHONY: all $(KBUILD_TARGETS)

all: modules

$(KBUILD_TARGETS):
	$(MAKE) -C $(KDIR) M="$(shell pwd)" $@
