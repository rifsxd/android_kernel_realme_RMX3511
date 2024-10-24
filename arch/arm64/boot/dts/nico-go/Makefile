# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtbo-$(CONFIG_ARCH_SPRD) += \
	ums9230-nico_go-overlay.dtbo

ums9230-nico_go-overlay.dtbo-base := ums9230.dtb
endif

endif #//BSP_BUILD_ANDROID_OS

always		:= $(dtb-y) $(dtbo-y)
clean-files	:= *.dtb *.dtbo
