#
# sprd_vdsp.ko
#
# Kbuild: for kernel building external module
#
# Note:
# - Please refer to modules/sample/Kbuild to find out what should be
#   done is this Kbuild file
#


ifeq ($(BSP_BOARD_PRODUCT_USING_VDSP),sharkl5pro)
PROJ_DIR := sharkl5pro
CONFIG_NEWIOMMUANDMEM := y
VERSION_DIR := v1
ccflags-y += -DMYL5
else ifeq ($(BSP_BOARD_PRODUCT_USING_VDSP),qogirn6pro)
PROJ_DIR := qogirn6pro
VERSION_DIR := v2
CONFIG_NEWIOMMUANDMEM := y
ccflags-y += -DMYN6
endif
# iommu dirver use signal iova
ccflags-y += -DVDSP_IOMMU_USE_SIGNAL_IOVA
ccflags-y += -DDEBUG
ccflags-y += -g
#
# Source List
#
KO_MODULE_NAME := sprd_vdsp
KO_MODULE_PATH := $(src)
KO_MODULE_SRC  :=

KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/products/$(PROJ_DIR)/*.c)




ifneq ($(BSP_BOARD_NAME),ums9620_10h10)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/$(VDSP_DIR)/$(PROJ_DIR)/vdsp_trusty.c)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/$(VDSP_DIR)/$(PROJ_DIR)/xrp_faceid.c)
endif


#
# Build Options
#
# ccflags-y += -DDEBUG
# ccflags-y += -g
ccflags-y += -DKERNEL_VERSION_54
ccflags-y += -I$(KO_MODULE_PATH)/../interface

ifneq ($(CONFIG_NEWIOMMUANDMEM),y)
ccflags-y += -I$(srctree)/drivers/staging/android/ion
endif

ifeq ($(BSP_BOARD_PRODUCT_USING_VDSP),sharkl5pro)
ccflags-y += -I$(srctree)/drivers/devfreq/apsys/
ccflags-y += -I$(srctree)/drivers/devfreq/apsys/vdsp
ccflags-y += -I$(KO_MODULE_PATH)/communication/ipi/
endif

ccflags-y += -I$(KO_MODULE_PATH)/products/$(PROJ_DIR)/
ccflags-y += -I$(KO_MODULE_PATH)/
ccflags-y += -I$(KO_MODULE_PATH)/dvfs/$(VERSION_DIR)/
ccflags-y += -I$(KO_MODULE_PATH)/inc/

ifneq ($(CONFIG_NEWIOMMUANDMEM),y)
ccflags-y += -I$(KO_MODULE_PATH)/smem/$(VERSION_DIR)/
ccflags-y += -I$(KO_MODULE_PATH)/trusty/$(VERSION_DIR)/
else
ccflags-y += -I$(KO_MODULE_PATH)/trusty/v2-iommuandmem/
endif

ifeq ($(BSP_BOARD_PRODUCT_USING_VDSP),qogirn6pro)
ccflags-y += -I$(KO_MODULE_PATH)/../../../common/camera/mmdvfs/r2p0/dvfs_driver/dvfs_reg_param/qogirn6pro/
ccflags-y += -I$(KO_MODULE_PATH)/../../../common/camera/mmdvfs/r2p0/mmsys_comm/
ccflags-y += -I$(KO_MODULE_PATH)/../../../common/camera/common
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(KO_MODULE_PATH)/communication/mailbox/
endif

# FaceID build options
# $(warning BSP_BOARD_NAME=$(BSP_BOARD_NAME))
# ifneq ($(BSP_BOARD_NAME),ums9620_10h10)
# ccflags-y += -DFACEID_VDSP_FULL_TEE
# endif

ifeq ($(CONFIG_COMPAT),y)
ccflags-y += -DCONFIG_COMPAT
endif

ifeq ($(TEST_ON_HAPS),true)
ccflags-y += -DTEST_ON_HAPS
endif



#
# iommu version check
#
$(warning BSP_DTB=$(BSP_DTB))
ifneq ($(findstring ums512,$(BSP_DTB)),)
#ifeq ($(BSP_DTB),ums512)
ccflags-y +=-DBSP_DTB_SHARKL5PRO
else ifneq ($(findstring ums9620,$(BSP_DTB)),)
ccflags-y +=-DBSP_DTB_QOGIRN6PRO
else
$(error "no match BSP_DTB_xxx")
endif

ifeq ($(CONFIG_NEWIOMMUANDMEM),y)
ccflags-y +=-DIOMMUANDMEM
ccflags-y += -I$(KO_MODULE_PATH)/iommu
ccflags-y += -I$(KO_MODULE_PATH)/memory


KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/iommu/*.c)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/memory/*.c)

# KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/trusty/v2-iommuandmem/xrp_faceid.c)
ccflags-y += -I$(KO_MODULE_PATH)/
ccflags-y += -I$(KO_MODULE_PATH)/mainflow/v2-iommuandmem/
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/mainflow/v2-iommuandmem/xvp_main.c)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/trusty/v2-iommuandmem/vdsp_trusty.c)

ccflags-y += -I$(KO_MODULE_PATH)/
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/*.c)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/dvfs/$(VERSION_DIR)/vdsp_dvfs.c)



ifeq ($(BSP_BOARD_PRODUCT_USING_VDSP),sharkl5pro) # sharkl5pro
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/communication/ipi/vdsp_ipi_drv.c)
else ifeq ($(BSP_BOARD_PRODUCT_USING_VDSP),qogirn6pro) # qogirn6pro
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/communication/mailbox/*.c)
endif

else
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/trusty/$(VERSION_DIR)/xrp_faceid.c)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/mainflow/$(VERSION_DIR)/xvp_main.c)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/smem/$(VERSION_DIR)/vdsp_smem.c)
KO_MODULE_SRC += $(wildcard $(KO_MODULE_PATH)/trusty/$(VERSION_DIR)/vdsp_trusty.c)
endif


#
# Final Objects
#
obj-m := $(KO_MODULE_NAME).o
# Comment it if the only object file has the same name with module
$(KO_MODULE_NAME)-y := $(patsubst $(src)/%.c,%.o,$(KO_MODULE_SRC))