#
# chipone-tddi.ko
#
# Makefile: for external make invocation
#
# Note:
# - Please refer to modules/sample/Makefile to find out what should be
#   done in this Makefile
#


#KO_MODULE_NAME := chipone-tddi
#KO_MODULE_OUT := $(BSP_MODULES_OUT)/$(KO_MODULE_NAME)
#KO_MODULE_KBUILD := $(CURDIR)/Kbuild

#.PHONY: modules modules_install clean
#modules:
#	@ln -snf $(CURDIR) $(KO_MODULE_OUT)/source
#	$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) src=$(CURDIR) $@

#modules_install:
#	$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) $@

## Remove the out directory wholly
#clean:
#	@#$(MAKE) -C $(BSP_KERNEL_PATH) M=$(KO_MODULE_OUT) src=$(CURDIR) $@
#	rm -rf $(KO_MODULE_OUT)

sprd-cts-y += \
	cts_charger_detect.o \
	cts_core.o \
	cts_earjack_detect.o \
	cts_firmware.o \
	cts_i2c_driver.o \
	cts_oem_huaqin.o \
	cts_platform.o \
	cts_sfctrlv2.o \
	cts_spi_flash.o \
	cts_strerror.o \
	cts_sysfs.o \
	cts_test.o \
	cts_tool.o \
	cts_proc.o

obj-m += sprd-cts.o