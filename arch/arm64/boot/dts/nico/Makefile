# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums9230-nico-overlay-evb.dtbo \
	ums9230-nico-overlay-t0-1.dtbo \
	ums9230-nico-overlay-t0-2.dtbo \
	ums9230-nico-overlay-evt1.dtbo \
	ums9230-nico-overlay-dvt.dtbo \
	ums9230-nico-overlay-pvt.dtbo

ums9230-nico-overlay.dtbo-base := ums9230.dtb
ums9230-nico-overlay-evb.dtbo-base := ums9230.dtb
ums9230-nico-overlay-t0-1.dtbo-base := ums9230.dtb
ums9230-nico-overlay-t0-2.dtbo-base := ums9230.dtb
ums9230-nico-overlay-evt1.dtbo-base := ums9230.dtb
ums9230-nico-overlay-dvt.dtbo-base := ums9230.dtb
ums9230-nico-overlay-pvt.dtbo-base := ums9230.dtb

endif

endif #//BSP_BUILD_ANDROID_OS

always		:= $(dtb-y) $(dtbo-y)
clean-files	:= *.dtb *.dtbo
