#
# Realtek Semiconductor Corp.
#
# July.22, 2016		Jim Cao (jim_cao@realsil.com.cn)
# Dec.6, 2016		Lei Wang(lei_wang@realsil.com.cn)
#

obj-$(CONFIG_SND_SOC_RTS_DMA) += rts_dma.o
obj-$(CONFIG_SND_SOC_RTS_I2S) += rts_i2s.o

obj-$(CONFIG_SND_SOC_RTS_INTERN_CODEC) += rts_codec.o
obj-$(CONFIG_SND_SOC_RTS_INTERN_CODEC) += rts_snd_intern.o
ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC
ccflags-y += -DCONFIG_SND_SOC_RTS_INTERN_CODEC
ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC_LINEIN
ccflags-y += -DCONFIG_SND_SOC_RTS_INTERN_CODEC_LINEIN
endif
ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC_DMIC
ccflags-y += -DCONFIG_SND_SOC_RTS_INTERN_CODEC_DMIC
endif
ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC_AMIC
ccflags-y += -DCONFIG_SND_SOC_RTS_INTERN_CODEC_AMIC
ifdef CONFIG_SND_SOC_RTS_AMIC_IN_LDO
ccflags-y += -DCONFIG_SND_SOC_RTS_AMIC_IN_LDO
endif
ifdef CONFIG_SND_SOC_RTS_AMIC_PMU_RTP
ccflags-y += -DCONFIG_SND_SOC_RTS_AMIC_PMU_RTP
ifdef CONFIG_SND_SOC_RTS_AMIC_PMU_RTP_LDO1
ccflags-y += -DCONFIG_SND_SOC_RTS_AMIC_PMU_RTP_LDO1
endif
endif #if CONFIG_SND_SOC_RTS_AMIC_PMU_RTP
endif #if CONFIG_SND_SOC_RTS_INTERN_CODEC_AMIC
endif #if CONFIG_SND_SOC_RTS_INTERN_CODEC

ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
ccflags-y += -DCONFIG_SND_SOC_RTS_EXTERN_CODEC
ifdef CONFIG_SND_SOC_RTS_RT5677
ccflags-y += -DCONFIG_SND_SOC_RTS_RT5677
endif
endif

obj-$(CONFIG_SND_SOC_RTS_QC) += rts_snd_qc.o

snd-soc-rt5651-objs := codec/rt5651.o codec/rt5651_ioctl.o codec/rt56xx_ioctl.o
obj-$(CONFIG_SND_SOC_RTS_RT5651) += snd-soc-rt5651.o
obj-$(CONFIG_SND_SOC_RTS_RT5651) += rts_snd_rt5651.o

obj-$(CONFIG_SND_SOC_RTS_RT5659) += rts_snd_rt5659.o

obj-$(CONFIG_SND_SOC_RTS_RT5514) += rts_snd_rt5514.o

obj-$(CONFIG_SND_SOC_RTS_RT5679) += codec/rt5679-spi.o
obj-$(CONFIG_SND_SOC_RTS_RT5679) += codec/rt5679.o
obj-$(CONFIG_SND_SOC_RTS_RT5679) += rts_snd_rt5679.o

snd-soc-rt5616-objs := codec/rt5616.o codec/rl6231.o
obj-$(CONFIG_SND_SOC_RTS_RT5616) += snd-soc-rt5616.o
obj-$(CONFIG_SND_SOC_RTS_RT5616) += rts_snd_rt5616.o

obj-$(CONFIG_SND_SOC_RTS_RT5677) += codec/rt5677-spi.o
obj-$(CONFIG_SND_SOC_RTS_RT5677) += codec/rt5677.o
obj-$(CONFIG_SND_SOC_RTS_RT5677) += rts_snd_rt5677.o

obj-$(CONFIG_SND_SOC_RTS_SPDIF)	+= rts_snd_spdif.o
ifdef CONFIG_SND_SOC_RTS_SPDIF
ccflags-y += -DCONFIG_SND_SOC_RTS_SPDIF
endif
