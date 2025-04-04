export CROSS_COMPILE=/home/cc/linuxworkfolder/tools/rk3588/rk3588_linux_sdk/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
KERNELDIR := /home/cc/linuxworkfolder/tools/rk3588/rk3588_linux_sdk/kernel
CURRENT_PATH := $(shell pwd)
obj-m := m_workqueue.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
