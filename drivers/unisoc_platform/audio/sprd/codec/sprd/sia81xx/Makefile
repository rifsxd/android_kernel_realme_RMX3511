# SIA81xx PA object
SPRD_SOUND_TREE := drivers/unisoc_platform/audio/sprd
ccflags-y += -I$(SPRD_SOUND_TREE)/codec/sprd

# COMPILE_TO_MODULE = y
EN_SIA81XX_TUNING = n
TUNING_PLATFORM = mtk

snd-soc-sia81xx-objs := \
		sia81xx.o \
		sia81xx_regmap.o \
		sia81xx_aux_dev_if.o \
		sia81xx_set_vdd.o \
		sia8101_regs.o \
		sia8109_regs.o \
		sia8152_regs.o \
		sia8152s_regs.o \
		sia8159_regs.o \
		sia8159a_regs.o \
		sia81xx_timer_task.o

ifeq ($(EN_SIA81XX_TUNING),y)
EXTRA_CFLAGS += -DSIA81XX_TUNING
snd-soc-sia81xx-objs += sia81xx_socket.o
endif

ifeq ($(TUNING_PLATFORM),qcom)
snd-soc-sia81xx-objs += sia81xx_afe.o
endif

ifeq ($(TUNING_PLATFORM),mtk)
snd-soc-sia81xx-objs += sia81xx_tuning_if_dev.o
endif

ifeq ($(TUNING_PLATFORM),jlq)
snd-soc-sia81xx-objs += sia81xx_tuning_if_dev.o	# temp use
endif

ifeq ($(CONFIG_SND_SOC_UNISOC_CODEC_SIA81XX),m)
EXTRA_CFLAGS += -DSIA81XX_COMPILE_TO_MODULE
endif

obj-$(CONFIG_SND_SOC_UNISOC_CODEC_SIA81XX) += snd-soc-sia81xx.o