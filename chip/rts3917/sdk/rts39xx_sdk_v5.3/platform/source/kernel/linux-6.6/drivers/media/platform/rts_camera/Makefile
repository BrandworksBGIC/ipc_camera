KSRC := $(DIR_LINUX)
ARCH := rlx
EXTRA_CFLAGS += -Werror -Wall
ifdef CONFIG_FW_IN_KERNEL_HEADER
	EXTRA_CFLAGS += -DCONFIG_LOAD_FW
endif
MODULE_RTS_CAMERA := rts_cam
obj-$(CONFIG_RTS_CAMERA) += $(MODULE_RTS_CAMERA).o
$(MODULE_RTS_CAMERA)-objs += rts_camera.o
$(MODULE_RTS_CAMERA)-objs += rts_camera_dev.o
$(MODULE_RTS_CAMERA)-objs += rts_camera_video.o
$(MODULE_RTS_CAMERA)-objs += rts_camera_ctrl.o
$(MODULE_RTS_CAMERA)-objs += rts_camera_fps.o

ifdef CONFIG_RTS_ISP_MEM
	MODULE_RTS_ISP_MEM := rts_cam_mem
	obj-$(CONFIG_RTS_ISP_MEM) += $(MODULE_RTS_ISP_MEM).o
	$(MODULE_RTS_ISP_MEM)-objs += rts_isp_mem.o
	$(MODULE_RTS_ISP_MEM)-objs += rts_camera_mem.o
	$(MODULE_RTS_ISP_MEM)-objs += rts-dma-contig.o
endif

ifdef CONFIG_RTS_CAM_LOCK
	MODULE_RTS_CAM_LOCK := rts_cam_lock
	obj-$(CONFIG_RTS_CAM_LOCK) += $(MODULE_RTS_CAM_LOCK).o
	$(MODULE_RTS_CAM_LOCK)-objs += rts_camera_lock.o
	$(MODULE_RTS_CAM_LOCK)-objs += rts_camera_waitqueue.o
endif

ifdef CONFIG_RTS_VIDEO_ISP
	MODULE_RTS_SOC := rts_cam_soc
	obj-$(CONFIG_RTS_VIDEO_ISP) += $(MODULE_RTS_SOC).o
	$(MODULE_RTS_SOC)-objs += rts_camera_soc_3915.o
	$(MODULE_RTS_SOC)-objs += rts_camera_soc.o

	MODULE_RTS_ZOOM := rts_cam_zoom
	obj-$(CONFIG_RTS_VIDEO_ISP) += $(MODULE_RTS_ZOOM).o
	$(MODULE_RTS_ZOOM)-objs += rts_camera_zoom.o

	MODULE_RTS_ISP := rts_cam_isp
	obj-$(CONFIG_RTS_VIDEO_ISP) += $(MODULE_RTS_ISP).o
	$(MODULE_RTS_ISP)-objs += rts_camera_isp.o

	MODULE_RTS_MD := rts_cam_md
	obj-$(CONFIG_RTS_VIDEO_ISP) += $(MODULE_RTS_MD).o
	$(MODULE_RTS_MD)-objs += rts_camera_md.o

endif

ifdef CONFIG_RTS_W521MP_ENC
	MODULE_VIDEO_W521MP_ENC := vpu_w521mp
	obj-$(CONFIG_RTS_W521MP_ENC) += $(MODULE_VIDEO_W521MP_ENC).o
endif

ifdef CONFIG_RTS_MJPEG_ENC
	MODULE_RTS_MJPEG_ENC := rts_camera_jpgenc
	obj-$(CONFIG_RTS_MJPEG_ENC) += $(MODULE_RTS_MJPEG_ENC).o
endif

ifdef CONFIG_RTS_OSD2
	MODULE_RTS_OSD2 := rts_camera_osd2
	obj-$(CONFIG_RTS_OSD2) += $(MODULE_RTS_OSD2).o
endif

ifdef CONFIG_RTS_OSDI
	MODULE_RTS_OSDI := rts_camera_osdi
	obj-$(CONFIG_RTS_OSDI) += $(MODULE_RTS_OSDI).o
endif

ifdef CONFIG_RTS_VIDEO_PP
	MODULE_RTS_PP := rts_pp
	obj-$(CONFIG_RTS_VIDEO_PP) += $(MODULE_RTS_PP).o
endif

ifdef CONFIG_RTS_BWT_MONITOR
	MODULE_RTS_BWT_MONITOR := rts_bwt_monitor
	obj-$(CONFIG_RTS_BWT_MONITOR) += $(MODULE_RTS_BWT_MONITOR).o
endif

ifdef CONFIG_RTS_RTSTREAM_INFO
	MODULE_RTS_RTSTREAM_INFO := rtstream
	obj-$(CONFIG_RTS_RTSTREAM_INFO) += $(MODULE_RTS_RTSTREAM_INFO).o
endif

