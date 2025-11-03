################################################################################
#
# rts_sample
#
################################################################################

RTS_SAMPLE_SITE_METHOD = local
RTS_SAMPLE_SITE = $(BR2_EXTERNAL)/source/rtscore/rts_sample
RTS_SAMPLE_CONF_OPTS += -DCMAKE_INSTALL_PREFIX="/"

RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_VO_AACENC), vo-aacenc)
RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_ALURE), alure)
RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_RTSSED), realvoice)
RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_LIBRTSCRYPT), librtscrypt)
RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_RTSTREAM), rtstream)
RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_LIBRTSIO), librtsio)
RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_FFMPEG), ffmpeg)
RTS_SAMPLE_DEPENDENCIES += $(if $(BR2_PACKAGE_RTS_FFMPEG), rts_ffmpeg)

ifeq ($(BR2_PACKAGE_FFMPEG), y)
ifeq ($(BR2_PACKAGE_RTS_SAMPLE), y)
FFMPEG_CONF_OPTS = \
	--prefix=/usr \
	--enable-cross-compile \
	--cross-prefix=$(TARGET_CROSS) \
	--sysroot=$(STAGING_DIR) \
	--host-cc="$(HOSTCC)" \
	--arch=$(BR2_ARCH) \
	--target-os="linux" \
	--extra-ldflags=-L$(STAGING_DIR)/usr/lib \
	--disable-iconv \
	--disable-mips32r2 \
	--disable-mipsdsp \
	--disable-mipsdspr2 \
	--disable-mipsfpu \
	--enable-shared \
	--enable-small \
	--disable-everything \
	--disable-programs \
	--disable-avdevice \
	--disable-swscale \
	--enable-avresample \
	--disable-avfilter \
	--disable-debug \
	--disable-doc	\
	--enable-protocol=file \
	--enable-muxer=mp4 \
	--enable-bsf=aac_adtstoasc \
	--enable-version3
endif
endif

RTS_SAMPLE_CONF_OPTS += \
	-DCONFIG_RTSTREAM_VIDEO=$(if $(BR2_PACKAGE_RTSTREAM_VIDEO),y,n) \
	-DCONFIG_RTSTREAM_AUDIO=$(if $(BR2_PACKAGE_RTSTREAM_AUDIO),y,n) \
	-DCONFIG_PACKAGE_h1_encoder=$(if $(BR2_PACKAGE_H1_ENCODER),y,n) \
	-DCONFIG_PACKAGE_H265=$(if $(BR2_PACKAGE_WAVE420),y,n) \
	-DCONFIG_PACKAGE_H26x=$(if $(BR2_PACKAGE_WAVE521),y,n) \
	-DCONFIG_PACKAGE_RTS_FFMPEG=$(if $(BR2_PACKAGE_RTS_FFMPEG),y,n) \
	-DCONFIG_PACKAGE_MCU=$(if $(BR2_PACKAGE_LIBRTSMCU),y,n) \
	-DCONFIG_PACKAGE_ISP_ALGO=$(if $(BR2_PACKAGE_ISP_ALGO),y,n) \
	-DCONFIG_INSTALL_SAMPLES=$(if $(BR2_INSTALL_RTS_SAMPLE),y,n) \
	-DCONFIG_SHARE_SAMPLE=$(if $(BR2_PACKAGE_RTS_SAMPLE_SHARE),y,n) \
	-DCONFIG_RTSTREAM_MTD2=$(if $(BR2_PACKAGE_RTSTREAM_MTD2),y,n) \
	-DCONFIG_RTSTREAM_VOUT=$(if $(BR2_PACKAGE_RTSTREAM_VIDEO_OUT),y,n) \
	-DCONFIG_RTSTREAM_OSD2_PPU=$(if $(BR2_PACKAGE_RTSTREAM_OSD2_PPU),y,n) \
	-DCONFIG_SAMPLE_RTSTREAM_AUDIO=$(if $(BR2_PACKAGE_RTS_SAMPLE_RTSTREAM_AUDIO),y,n) \
	-DCONFIG_SAMPLE_RTSTREAM_VIDEO=$(if $(BR2_PACKAGE_RTS_SAMPLE_RTSTREAM_VIDEO),y,n) \
	-DCONFIG_SAMPLE_RTSSED=$(if $(BR2_PACKAGE_RTS_SAMPLE_RTSSED),y,n) \
	-DCONFIG_SAMPLE_RTSAAC=$(if $(BR2_PACKAGE_RTS_SAMPLE_RTSAAC),y,n) \
	-DCONFIG_SAMPLE_RTSMP3=$(if $(BR2_PACKAGE_RTS_SAMPLE_RTSMP3),y,n) \
	-DCONFIG_SAMPLE_LIBRTSIO=$(if $(BR2_PACKAGE_RTS_SAMPLE_LIBRTSIO),y,n) \
	-DCONFIG_SAMPLE_LIBRTSCRYPT=$(if $(BR2_PACKAGE_RTS_SAMPLE_LIBRTSCRYPT),y,n) \
	-DDIR_TMPFS=$(STAGING_DIR)/usr

$(eval $(cmake-package))
