# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums9230-nicky-overlay-evb.dtbo \
	ums9230-nicky-overlay-t0-1.dtbo \
	ums9230-nicky-overlay-evt1.dtbo \
	ums9230-nicky-overlay-evt2.dtbo \
	ums9230-nicky-overlay-dvt.dtbo \
	ums9230-nicky-overlay-pvt.dtbo \
	ums9230-nicky-overlay-mp-0.dtbo \
	ums9230-nicky-overlay-mp-1.dtbo

ums9230-nicky-overlay-evb.dtbo-base := ums9230.dtb
ums9230-nicky-overlay-t0-1.dtbo-base := ums9230.dtb
ums9230-nicky-overlay-evt1.dtbo-base := ums9230.dtb
ums9230-nicky-overlay-evt2.dtbo-base := ums9230.dtb
ums9230-nicky-overlay-dvt.dtbo-base := ums9230.dtb
ums9230-nicky-overlay-pvt.dtbo-base := ums9230.dtb
ums9230-nicky-overlay-mp-0.dtbo-base := ums9230.dtb
ums9230-nicky-overlay-mp-1.dtbo-base := ums9230.dtb

endif

endif #//BSP_BUILD_ANDROID_OS

always		:= $(dtb-y) $(dtbo-y)
clean-files	:= *.dtb *.dtbo
