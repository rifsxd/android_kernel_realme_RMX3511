# Makefile for the focaltech touchscreen drivers.

focaltech_touch_milan-y += \
	touch.o \
	tp_pannel_notifier.o \
	tp_usb_notifier.o \
	focaltech_core.o \
	focaltech_ex_fun.o \
	focaltech_ex_mode.o \
	focaltech_gesture.o \
	focaltech_esdcheck.o \
	focaltech_point_report_check.o \
	focaltech_flash.o \
	focaltech_spi_v2.o \
	focaltech_spi.o \
	focaltech_test.o \
	focaltech_test_ini.o \
	focaltech_test_ft8006s_ab.o \
	focaltech_test_ft8722.o \
	focaltech_test_ft8615.o
$(warning errorlogxxxxlog:$(strip $(CONFIG_TOUCHSCREEN_FTS_MILAN_CHIPSET)))	
obj-$(CONFIG_TOUCHSCREEN_FTS_MILAN_CHIPSET)	+=  focaltech_touch_milan.o
# $(error errorlogxxxxlog:$(strip $(CONFIG_TOUCHSCREEN_FTS)))
