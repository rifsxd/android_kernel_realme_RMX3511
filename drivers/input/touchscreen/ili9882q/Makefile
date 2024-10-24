BUILD_INFAE := spi
BUILD_PLATFORM := qcom
BUILD_MODULE := y

ifeq ($(BUILD_PLATFORM),qcom)
platform=ilitek_v3_qcom
endif

ifeq ($(BUILD_INFAE),i2c)
interface=ilitek_v3_i2c
fwupdate=ilitek_v3_flash
endif

ifeq ($(BUILD_INFAE),spi)
interface=ilitek_v3_spi
fwupdate=ilitek_v3_hostdl
endif

ilitek-y := ilitek_v3.o \
		$(interface).o \
		$(platform).o \
		ilitek_v3_ic.o \
		ilitek_v3_touch.o \
		ilitek_v3_mp.o \
		$(fwupdate).o \
		ilitek_v3_node.o \
		ilitek_oppo_node.o \
		touch.o \
		tp_usb_notifier.o 
obj-m += ilitek.o

