//KDIR = $(HOME)/Kernels/linux-4.14.91
KDIR = $(HOME)/CreateImageOP/cache/sources/linux-mainline/linux-4.14.y
//KDIR = /lib/modules/$(shell uname -r)/build
ARCH = arm
CCFLAGS = -C
//COMPILER = arm-linux-gnueabihf-
COMPILER = arm-unknown-linux-gnueabihf-
PWD = $(shell pwd)
TARGET = leds-blink

obj-m   := $(TARGET).o

CFLAGS_$(TARGET_MOD).o := -DDEBUG

all:
	$(MAKE) $(CCFLAGS) $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(COMPILER) modules
	
copy_dtbo:
	@./mod.sh copy-dtbo
copy_dtb:
	@./mod.sh copy-dtb
del_mod:
	@./mod.sh delete-ko
copy_mod:
	@./mod.sh copy-ko
compile_dts:
	@./mod.sh compile-dts
compile_dtsi:
	@./mod.sh compile-dtsi
	
clean: 
	@rm -f *.o .*.cmd .*.flags *.mod.c *.order *.dwo *.mod.dwo .*.dwo
	@rm -f .*.*.cmd *~ *.*~ TODO.*
	@rm -fR .tmp* 
	@rm -rf .tmp_versions
	@rm -f *.ko *.symvers