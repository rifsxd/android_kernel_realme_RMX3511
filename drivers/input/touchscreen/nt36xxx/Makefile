#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.

sprd-nt36xxx-y += \
	touch.o \
	nt36xxx.o \
	nt36xxx_fw_update.o \
	nt36xxx_ext_proc.o \
	nt36xxx_mp_ctrlram.o

obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += sprd-nt36xxx.o
