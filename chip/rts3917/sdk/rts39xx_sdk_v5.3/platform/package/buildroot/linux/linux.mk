################################################################################
#
# linux custom script
#
################################################################################

LINUX_RLX_PATCHES := $(BR2_EXTERNAL_platform_PATH)/patches/linux-$(BR2_LINUX_KERNEL_VERSION) \

define LINUX_APPLY_LOCAL_RLX_PATCHES
	for p in $(LINUX_RLX_PATCHES) ; do \
		if test -d $$p ; then \
			$(APPLY_PATCHES) $(@D) $$p \*.patch || exit 1 ; \
		else \
			$(APPLY_PATCHES) $(@D) `dirname $$p` `basename $$p` || exit 1; \
		fi \
        done
endef

LINUX_POST_RSYNC_HOOKS += LINUX_APPLY_LOCAL_RLX_PATCHES

ifeq ($(BR2_LINUX_KERNEL_DTS_SUPPORT),y)

ifneq ($(BR2_LINUX_KERNEL_INTREE_DTS_NAME),"")
DTS_NAME = $(call qstrip,$(BR2_LINUX_KERNEL_INTREE_DTS_NAME))
DTS_FILE = $(subst realtek/,,$(DTS_NAME))
LINUX_DTS_PATH = $(LINUX_DIR)/arch/arm/boot/dts/realtek
endif
ifneq ($(BR2_LINUX_KERNEL_CUSTOM_DTS_PATH),"")
DTS_FILE = $(basename $(filter %.dts,$(notdir $(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_DTS_PATH)))))
LINUX_DTS_PATH = $(LINUX_DIR)/arch/arm/boot/dts
endif

endif

ifeq ($(BR2_LINUX_KERNEL_VERSION),"5.4")

ifeq ($(BR2_LINUX_KERNEL_DTB_CHECK),y)
KERNEL_BINDING_FILE = $(LINUX_DIR)/Documentation/devicetree/bindings/
depfile = $(LINUX_DTS_PATH)/.$(DTS_FILE).dt.yaml.d
dtc_cpp_flag = -Wp,-MD,$(depfile).pre.tmp -nostdinc \
		 -nostdinc -I.$(depfile).pre.tmp/scripts/dtc/include-prefixes \
		 -I  $(LINUX_DIR)/include/ -undef -D__DTS__
DTC_INCLUDE	:= $(LINUX_DIR)/scripts/dtc/include-prefixes
DTC_FLAGS += -Wno-unit_address_vs_reg \
-Wno-unit_address_format \
-Wno-avoid_unnecessary_addr_size \
-Wno-alias_paths \
-Wno-graph_child_address \
-Wno-simple_bus_reg \
-Wno-unique_unit_address \
-Wno-pci_device_reg
dtc-tmp = $(LINUX_DTS_PATH)/.$(DTS_FILE).dt.yaml.dts.tmp
dtc_tool = $(LINUX_DIR)/scripts/dtc/dtc
dtc-path = $(LINUX_DIR)/scripts/dtc
dt-mk-schema = $(HOST_DIR)/bin/dt-mk-schema
dt-validate = $(HOST_DIR)/bin/dt-validate
dtc_flag = -Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu89 -I $(LINUX_DIR)/scripts/dtc/libfdt -I $(HOST_DIR)/include/
dtc-file := dtc flattree fstree data livetree treesource srcpos checks util dtc-lexer.lex dtc-parser.tab yamltree
yaml_file = $(LINUX_DTS_PATH)/$(DTS_FILE).dt.yaml
define LINUX_DTBS_CHECK
	flex -o $(dtc-path)/dtc-lexer.lex.c -L $(dtc-path)/dtc-lexer.l
	bison -o $(dtc-path)/dtc-parser.tab.c --defines=$(dtc-path)/dtc-parser.tab.h -t -l $(dtc-path)/dtc-parser.y
	$(foreach n,$(dtc-file), $(HOSTCC) -Wp,-MD,$(dtc-path)/.$(n).o.d $(dtc_flag) -c -o $(dtc-path)/$(n).o $(dtc-path)/$(n).c;)
  	$(HOSTCC) -o $(dtc-path)/dtc $(dtc-path)/dtc.o $(dtc-path)/flattree.o $(dtc-path)/fstree.o $(dtc-path)/data.o $(dtc-path)/livetree.o $(dtc-path)/treesource.o \
	  $(dtc-path)/srcpos.o $(dtc-path)/checks.o $(dtc-path)/util.o $(dtc-path)/dtc-lexer.lex.o $(dtc-path)/dtc-parser.tab.o \
	  $(dtc-path)/yamltree.o  -L $(HOST_DIR)/lib/ -lyaml
	$(dt-mk-schema) -o $(LINUX_DIR)/Documentation/devicetree/bindings/processed-schema.yaml $(shell (find $(BR2_EXTERNAL)/source/kernel/linux-5.4/ -name "*.yaml" |sort)) ;
	$(HOSTCC) -E $(dtc_cpp_flag) -x assembler-with-cpp -o $(dtc-tmp) $(LINUX_DTS_PATH)/$(DTS_FILE).dts ; \
	$(dtc_tool) -O yaml -o $(yaml_file) -b 0 \
		$(addprefix -i,$(dir $<) $(DTC_INCLUDE)) $(DTC_FLAGS) \
		-d $(depfile).dtc.tmp $(dtc-tmp) ; \
	cat $(depfile).pre.tmp $(depfile).dtc.tmp > $(depfile)
	$(dt-validate) -u $(LINUX_DIR)/Documentation/devicetree/bindings -p \
		$(LINUX_DIR)/Documentation/devicetree/bindings/processed-schema.yaml $(LINUX_DTS_PATH)/$(DTS_FILE).dt.yaml ;
endef
endif


define LINUX_INSTALL_VIDEO_HEADERS
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done

	if grep -q -E "^CONFIG_RTS_W420_ENC=y|^CONFIG_RTS_W420_ENC=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux/wave420 -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done\
	fi

	if grep -q -E "^CONFIG_RTS_W521_ENC=y|^CONFIG_RTS_W521_ENC=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux/wave521 -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done\
	fi

	if grep -q -E "^CONFIG_RTS_W521MP_ENC=y|^CONFIG_RTS_W521MP_ENC=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux/wave521mp -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done\
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += LINUX_INSTALL_VIDEO_HEADERS

KMOD_VIDEO_SCRIPT_NAME = 07_init_video

define KMOD_VIDEO_INSTALL_MISC
	if [ -e $(@D)/$(KMOD_VIDEO_SCRIPT_NAME) ]; then \
		rm $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_CAMERA=m $(@D)/.config ; then \
		echo "#!/bin/sh" > $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "#this is automatically generated" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo -e "\n" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_ISP_MEM=m $(@D)/.config ; then \
		echo "modprobe rts_cam_mem.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_CAM_LOCK=m $(@D)/.config ; then \
		echo "modprobe rts_cam_lock.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_VIDEO_MCU=m $(@D)/.config ; then \
		echo "modprobe rts_cam_soc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_mcu.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_VIDEO_ISP=m $(@D)/.config ; then \
		echo "modprobe rts_cam_soc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_zoom.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_isp.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_md.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_HX280_ENC=m $(@D)/.config ; then \
		echo "modprobe rts_camera_hx280enc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_W420_ENC=m $(@D)/.config ; then \
		echo "modprobe vpu_w420.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_W521_ENC=m $(@D)/.config ; then \
		echo "modprobe vpu_w521.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_W521MP_ENC=m $(@D)/.config ; then \
		echo "modprobe vpu_w521mp.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_MJPEG_ENC=m $(@D)/.config ; then \
		echo "modprobe rts_camera_jpgenc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_MJPEG_CODEC=m $(@D)/.config ; then \
		echo "modprobe jpu.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_OSD2=m $(@D)/.config ; then \
		echo "modprobe rts_camera_osd2.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_OSDI=m $(@D)/.config ; then \
		echo "modprobe rts_camera_osdi.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_BWT_MONITOR=m $(@D)/.config ; then \
		echo "modprobe rts_bwt_monitor.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_RTSTREAM_INFO=m $(@D)/.config ; then \
		echo "modprobe rtstream.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if [ -e $(@D)/$(KMOD_VIDEO_SCRIPT_NAME) ]; then \
		$(INSTALL) -D -m 755  $(@D)/$(KMOD_VIDEO_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += KMOD_VIDEO_INSTALL_MISC

KMOD_AUDIO_SCRIPT_NAME = 03_init_audio

define KMOD_AUDIO_INSTALL_MISC
	if [ -e $(@D)/$(KMOD_AUDIO_SCRIPT_NAME) ]; then \
		rm $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q "^CONFIG_SND_SOC_RTS_.*=m" $(@D)/.config ; then \
		echo "#!/bin/sh" > $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "#this is automatically generated" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "#please ensure rts_snd_xx.ko is inserted at last" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo -e "\n" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_dma.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_i2s.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_INTERN_CODEC=m $(@D)/.config ; then \
		echo "modprobe rts_codec.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_intern.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_SPDIF=m $(@D)/.config ; then \
		echo "modprobe rts_snd_spdif.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_QC=m $(@D)/.config ; then \
		echo "modprobe rts_snd_qc.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5659=m $(@D)/.config ; then \
		echo "modprobe rts_snd_rt5659.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5514=m $(@D)/.config ; then \
		echo "modprobe rts_snd_rt5514.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5651=m $(@D)/.config ; then \
		echo "modprobe snd-soc-rt5651.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5651.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5616=m $(@D)/.config ; then \
		echo "modprobe snd-soc-rt5616.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5616.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5679=m $(@D)/.config ; then \
		echo "modprobe rt5679-spi.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rt5679.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5679.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5677=m $(@D)/.config ; then \
		echo "modprobe rt5677-spi.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rt5677.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5677.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q -E "^CONFIG_SND_SOC_RTS_RT5677=y|^CONFIG_SND_SOC_RTS_RT5677=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/sound/soc/realtek/codec/ -name "*.bin"`; do \
		$(INSTALL)  -D -m 644 $$p $(TARGET_DIR)/lib/firmware;\
	done\
	fi

	if [ -e $(@D)/$(KMOD_AUDIO_SCRIPT_NAME) ]; then \
		$(INSTALL) -D -m 755  $(@D)/$(KMOD_AUDIO_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += KMOD_AUDIO_INSTALL_MISC

KMOD_SDHC_SCRIPT_NAME = 03_init_sdhc

define KMOD_SDHC_INSTALL_MISC
	if [ -e $(@D)/$(KMOD_SDHC_SCRIPT_NAME) ]; then \
		rm $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
	fi
	if grep -q "^CONFIG_MMC_REALTEK_IPCAM=m" $(@D)/.config ; then \
		echo "#!/bin/sh" > $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		echo "#this is automatically generated" >> $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		echo -e "\n" >> $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		echo "modprobe rtsx-icr.ko" >> $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		$(INSTALL) -D -m 755  $(@D)/$(KMOD_SDHC_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_SDHC_SCRIPT_NAME); \
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += KMOD_SDHC_INSTALL_MISC

# trick to avoid linux build always run
define LINUX_SYNC_STAMPS
	touch $(@D)/.config
	touch $(@D)/.stamp_*
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += LINUX_SYNC_STAMPS

endif

ifeq ($(BR2_LINUX_KERNEL_VERSION),"6.6")

ifeq ($(BR2_LINUX_KERNEL_DTB_CHECK),y)
KERNEL_BINDING_FILE = $(LINUX_DIR)/Documentation/devicetree/bindings/
depfile = $(LINUX_DTS_PATH)/.$(DTS_FILE).dt.yaml.d
dtc_cpp_flag = -Wp,-MD,$(depfile).pre.tmp -nostdinc \
		 -nostdinc -I.$(depfile).pre.tmp/scripts/dtc/include-prefixes \
		 -I  $(LINUX_DIR)/include/ -undef -D__DTS__
DTC_INCLUDE	:= $(LINUX_DIR)/scripts/dtc/include-prefixes
DTC_FLAGS += -Wno-unit_address_vs_reg \
-Wno-unit_address_format \
-Wno-avoid_unnecessary_addr_size \
-Wno-alias_paths \
-Wno-graph_child_address \
-Wno-simple_bus_reg \
-Wno-unique_unit_address \
-Wno-pci_device_reg
dtc-tmp = $(LINUX_DTS_PATH)/.$(DTS_FILE).dt.yaml.dts.tmp
dtc_tool = $(LINUX_DIR)/scripts/dtc/dtc
dtc-path = $(LINUX_DIR)/scripts/dtc
dt-mk-schema = $(HOST_DIR)/bin/dt-mk-schema
dt-validate = $(HOST_DIR)/bin/dt-validate
dtc_flag = -Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu89 -I $(LINUX_DIR)/scripts/dtc/libfdt -I $(HOST_DIR)/include/
dtc-file := dtc flattree fstree data livetree treesource srcpos checks util dtc-lexer.lex dtc-parser.tab yamltree
yaml_file = $(LINUX_DTS_PATH)/$(DTS_FILE).dt.yaml
define LINUX_DTBS_CHECK
	flex -o $(dtc-path)/dtc-lexer.lex.c -L $(dtc-path)/dtc-lexer.l
	bison -o $(dtc-path)/dtc-parser.tab.c --defines=$(dtc-path)/dtc-parser.tab.h -t -l $(dtc-path)/dtc-parser.y
	$(foreach n,$(dtc-file), $(HOSTCC) -Wp,-MD,$(dtc-path)/.$(n).o.d $(dtc_flag) -c -o $(dtc-path)/$(n).o $(dtc-path)/$(n).c;)
	$(HOSTCC) -o $(dtc-path)/dtc $(dtc-path)/dtc.o $(dtc-path)/flattree.o $(dtc-path)/fstree.o $(dtc-path)/data.o $(dtc-path)/livetree.o $(dtc-path)/treesource.o \
	  $(dtc-path)/srcpos.o $(dtc-path)/checks.o $(dtc-path)/util.o $(dtc-path)/dtc-lexer.lex.o $(dtc-path)/dtc-parser.tab.o \
	  $(dtc-path)/yamltree.o  -L $(HOST_DIR)/lib/ -lyaml
	$(dt-mk-schema) -o $(LINUX_DIR)/Documentation/devicetree/bindings/processed-schema.yaml $(shell (find $(BR2_EXTERNAL)/source/kernel/linux-5.4/ -name "*.yaml" |sort)) ;
	$(HOSTCC) -E $(dtc_cpp_flag) -x assembler-with-cpp -o $(dtc-tmp) $(LINUX_DTS_PATH)/$(DTS_FILE).dts ; \
	$(dtc_tool) -O yaml -o $(yaml_file) -b 0 \
		$(addprefix -i,$(dir $<) $(DTC_INCLUDE)) $(DTC_FLAGS) \
		-d $(depfile).dtc.tmp $(dtc-tmp) ; \
	cat $(depfile).pre.tmp $(depfile).dtc.tmp > $(depfile)
	$(dt-validate) -u $(LINUX_DIR)/Documentation/devicetree/bindings -p \
		$(LINUX_DIR)/Documentation/devicetree/bindings/processed-schema.yaml $(LINUX_DTS_PATH)/$(DTS_FILE).dt.yaml ;
endef
endif


define LINUX_INSTALL_VIDEO_HEADERS
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done

	if grep -q -E "^CONFIG_RTS_W420_ENC=y|^CONFIG_RTS_W420_ENC=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux/wave420 -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done\
	fi

	if grep -q -E "^CONFIG_RTS_W521_ENC=y|^CONFIG_RTS_W521_ENC=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux/wave521 -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done\
	fi

	if grep -q -E "^CONFIG_RTS_W521MP_ENC=y|^CONFIG_RTS_W521MP_ENC=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/drivers/media/platform/rts_camera/linux/wave521mp -name "*.h"`; do \
		$(INSTALL)  -D -m 644 $$p $(STAGING_DIR)/usr/include;\
	done\
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += LINUX_INSTALL_VIDEO_HEADERS

KMOD_VIDEO_SCRIPT_NAME = 07_init_video

define KMOD_VIDEO_INSTALL_MISC
	if [ -e $(@D)/$(KMOD_VIDEO_SCRIPT_NAME) ]; then \
		rm $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_CAMERA=m $(@D)/.config ; then \
		echo "#!/bin/sh" > $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "#this is automatically generated" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo -e "\n" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_ISP_MEM=m $(@D)/.config ; then \
		echo "modprobe rts_cam_mem.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_CAM_LOCK=m $(@D)/.config ; then \
		echo "modprobe rts_cam_lock.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_VIDEO_MCU=m $(@D)/.config ; then \
		echo "modprobe rts_cam_soc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_mcu.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_VIDEO_ISP=m $(@D)/.config ; then \
		echo "modprobe rts_cam_soc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_zoom.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_isp.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
		echo "modprobe rts_cam_md.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_HX280_ENC=m $(@D)/.config ; then \
		echo "modprobe rts_camera_hx280enc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_W420_ENC=m $(@D)/.config ; then \
		echo "modprobe vpu_w420.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_W521_ENC=m $(@D)/.config ; then \
		echo "modprobe vpu_w521.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_W521MP_ENC=m $(@D)/.config ; then \
		echo "modprobe vpu_w521mp.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_MJPEG_ENC=m $(@D)/.config ; then \
		echo "modprobe rts_camera_jpgenc.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_MJPEG_CODEC=m $(@D)/.config ; then \
		echo "modprobe jpu.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_OSD2=m $(@D)/.config ; then \
		echo "modprobe rts_camera_osd2.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_OSDI=m $(@D)/.config ; then \
		echo "modprobe rts_camera_osdi.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_BWT_MONITOR=m $(@D)/.config ; then \
		echo "modprobe rts_bwt_monitor.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_RTS_RTSTREAM_INFO=m $(@D)/.config ; then \
		echo "modprobe rtstream.ko" >> $(@D)/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
	if [ -e $(@D)/$(KMOD_VIDEO_SCRIPT_NAME) ]; then \
		$(INSTALL) -D -m 755  $(@D)/$(KMOD_VIDEO_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_VIDEO_SCRIPT_NAME); \
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += KMOD_VIDEO_INSTALL_MISC

KMOD_AUDIO_SCRIPT_NAME = 03_init_audio

define KMOD_AUDIO_INSTALL_MISC
	if [ -e $(@D)/$(KMOD_AUDIO_SCRIPT_NAME) ]; then \
		rm $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q "^CONFIG_SND_SOC_RTS_.*=m" $(@D)/.config ; then \
		echo "#!/bin/sh" > $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "#this is automatically generated" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "#please ensure rts_snd_xx.ko is inserted at last" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo -e "\n" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_dma.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_i2s.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_INTERN_CODEC=m $(@D)/.config ; then \
		echo "modprobe rts_codec.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_intern.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_SPDIF=m $(@D)/.config ; then \
		echo "modprobe rts_snd_spdif.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_QC=m $(@D)/.config ; then \
		echo "modprobe rts_snd_qc.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5659=m $(@D)/.config ; then \
		echo "modprobe rts_snd_rt5659.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5514=m $(@D)/.config ; then \
		echo "modprobe rts_snd_rt5514.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5651=m $(@D)/.config ; then \
		echo "modprobe snd-soc-rt5651.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5651.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5616=m $(@D)/.config ; then \
		echo "modprobe snd-soc-rt5616.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5616.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5679=m $(@D)/.config ; then \
		echo "modprobe rt5679-spi.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rt5679.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5679.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q ^CONFIG_SND_SOC_RTS_RT5677=m $(@D)/.config ; then \
		echo "modprobe rt5677-spi.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rt5677.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
		echo "modprobe rts_snd_rt5677.ko" >> $(@D)/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
	if grep -q -E "^CONFIG_SND_SOC_RTS_RT5677=y|^CONFIG_SND_SOC_RTS_RT5677=m" $(@D)/.config ; then \
	for p in `find $(BUILD_DIR)/linux-custom/sound/soc/realtek/codec/ -name "*.bin"`; do \
		$(INSTALL)  -D -m 644 $$p $(TARGET_DIR)/lib/firmware;\
	done\
	fi

	if [ -e $(@D)/$(KMOD_AUDIO_SCRIPT_NAME) ]; then \
		$(INSTALL) -D -m 755  $(@D)/$(KMOD_AUDIO_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_AUDIO_SCRIPT_NAME); \
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += KMOD_AUDIO_INSTALL_MISC

KMOD_SDHC_SCRIPT_NAME = 03_init_sdhc

define KMOD_SDHC_INSTALL_MISC
	if [ -e $(@D)/$(KMOD_SDHC_SCRIPT_NAME) ]; then \
		rm $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
	fi
	if grep -q "^CONFIG_MMC_REALTEK_IPCAM=m" $(@D)/.config ; then \
		echo "#!/bin/sh" > $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		echo "#this is automatically generated" >> $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		echo -e "\n" >> $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		echo "modprobe rtsx-icr.ko" >> $(@D)/$(KMOD_SDHC_SCRIPT_NAME); \
		$(INSTALL) -D -m 755  $(@D)/$(KMOD_SDHC_SCRIPT_NAME) $(TARGET_DIR)/etc/preinit/$(KMOD_SDHC_SCRIPT_NAME); \
	fi
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += KMOD_SDHC_INSTALL_MISC

# trick to avoid linux build always run
define LINUX_SYNC_STAMPS
	touch $(@D)/.config
	touch $(@D)/.stamp_*
endef

LINUX_POST_INSTALL_IMAGES_HOOKS += LINUX_SYNC_STAMPS

endif

define MK_PARSER_LINUX_DTB
	# leave this line empty or commented
	$(call MESSAGE,"parser linux dtb")
	$(HOST_MAKE_ENV) dtc -I dtb -o ${LINUX_DTS_PATH}/$(DTS_FILE).dts.tmp \
		${LINUX_DTS_PATH}/$(DTS_FILE).dtb
	$(HOST_MAKE_ENV) $(BR2_EXTERNAL_platform_PATH)/scripts/part.py todts \
		$(LINUX_DTS_PATH)/$(DTS_FILE).dts.tmp \
		$(LINUX_DTS_PATH)/partitions.dts
endef
LINUX_BUILD_DTB += $(call MK_PARSER_LINUX_DTB)


ifeq ($(BR2_CONFIG_CRYPTO_BOOT),y)

define MK_CRYPTO_IMAGE
	if [[ $(CRYPTO_CIPHER) =~ cbc ]]; then \
		openssl enc $(CRYPTO_CIPHER) -in $(1) \
			-out $(1)$(CRYPTED) \
			-K `cat $(RTSKEY_OUT_DIR)/$(CRYPTO_KEY) | xxd -ps -c $(CRYPTO_KEYSIZE)` \
			-iv `cat $(RTSKEY_OUT_DIR)/$(CRYPTO_IV) | xxd -ps -c 16`; \
	else \
		openssl enc $(CRYPTO_CIPHER) -in $(1) \
			-out $(1)$(CRYPTED) \
			-K `cat $(RTSKEY_OUT_DIR)/$(CRYPTO_KEY) | xxd -ps -c $(CRYPTO_KEYSIZE)`; \
	fi
endef

define MK_LINUX_CRYPTO_IMAGE

	$(call MESSAGE,"Generating zImage$(CRYPTED)")
	rsync -a $(LINUX_DIR)/arch/arm/boot/zImage.dtb $(LINUX_DIR)/zImage
	$(call MK_CRYPTO_IMAGE, $(LINUX_DIR)/zImage)

	$(call MESSAGE,"Create linux uboot $(CRYPTED) image")
	$(HOST_DIR)/bin/mkimage \
		-n linux_`cat $(LINUX_DIR)/include/generated/utsrelease.h | awk '{print $$3; }' | sed 's/"//g'` \
		-A arm \
		-O linux \
		-T kernel \
		-C none \
		-a `cat $(LINUX_DIR)/arch/arm/mach-realtek/Makefile.boot | grep "loadaddr" | awk '{ print $$3 }'` \
		-e `cat $(LINUX_DIR)/arch/arm/mach-realtek/Makefile.boot | grep "loadaddr" | awk '{ print $$3 }'` \
		-d $(LINUX_DIR)/zImage$(CRYPTED) $(BINARIES_DIR)/uImage.dtb$(CRYPTED)
endef

LINUX_INSTALL_IMAGE += $(call MK_LINUX_CRYPTO_IMAGE)
endif

ifeq ($(BR2_CONFIG_SECURE_BOOT),y)
define MK_LINUX_SECURE_IMAGE

	$(call MESSAGE,"Create linux FIT image")
	rsync -a $(LINUX_DIR)/arch/arm/boot/zImage.dtb $(LINUX_DIR)/zImage
	sed "s%TAG_KERNEL_BIN%${LINUX_DIR}/zImage$(CRYPTED)%g; \
	s%TAG_LOAD_ADDR%`cat $(LINUX_DIR)/arch/arm/mach-realtek/Makefile.boot | grep "loadaddr" | awk '{ print $$3 }'`%g; \
	s%TAG_ENTRY_ADDR%`cat $(LINUX_DIR)/arch/arm/mach-realtek/Makefile.boot | grep "loadaddr" | awk '{ print $$3 }'`%g" \
		$(BR2_EXTERNAL_platform_PATH)/scripts/rtskey/linux.its.template > $(BINARIES_DIR)/linux.its
	PATH=$(HOST_DIR)/bin:$(PATH) $(HOST_DIR)/bin/mkimage \
		-f $(BINARIES_DIR)/linux.its \
		-k $(RTSKEY_OUT_DIR) \
		-r $(BINARIES_DIR)/linux$(CRYPTED).itb
endef

LINUX_INSTALL_IMAGE += $(call MK_LINUX_SECURE_IMAGE)
endif




# LINUX_KCONFIG_CUSTOM_FIXUP
ifneq ($(BR2_TARGET_ROOTFS_INITRAMFS),y)
LINUX_KCONFIG_DISABLE += CONFIG_BLK_DEV_INITRD
endif

ifeq ($(findstring y, $(BR2_CONFIG_SECURE_DM_MAPPER)$(BR2_CONFIG_FULL_DISK_ENCRYPTION)),y)
LINUX_KCONFIG_ENABLE += CONFIG_MD CONFIG_BLK_DEV_DM

ifeq ($(BR2_CONFIG_DM_INIT),y)
LINUX_KCONFIG_ENABLE += CONFIG_DM_INIT
else
LINUX_KCONFIG_DISABLE += CONFIG_DM_INIT
endif

else
LINUX_KCONFIG_DISABLE += CONFIG_MD CONFIG_BLK_DEV_DM
LINUX_KCONFIG_DISABLE += CONFIG_DM_INIT
endif

ifeq ($(BR2_CONFIG_SECURE_DM_MAPPER),y)
LINUX_KCONFIG_ENABLE += CONFIG_DM_VERITY

ifeq ($(BR2_CONFIG_SECURE_BOOT_USER_PARTITION)$(BR2_FW_PACK_USERDATA_EXT4),yy)
LINUX_KCONFIG_ENABLE += CONFIG_DM_INTEGRITY
else
LINUX_KCONFIG_DISABLE += CONFIG_DM_INTEGRITY
endif

else
LINUX_KCONFIG_DISABLE += CONFIG_DM_VERITY
LINUX_KCONFIG_DISABLE += CONFIG_DM_INTEGRITY

ifeq ($(BR2_CONFIG_UBIFS_FS_AUTHENTICATION),y)
LINUX_KCONFIG_ENABLE += CONFIG_UBIFS_FS_AUTHENTICATION
LINUX_KCONFIG_SET += CONFIG_SYSTEM_TRUSTED_KEYS
CONFIG_SYSTEM_TRUSTED_KEYS_VALUE = $(RTSKEY_OUT_DIR)/$(SECURE_DATA_CRT)
else
LINUX_KCONFIG_DISABLE += CONFIG_UBIFS_FS_AUTHENTICATION
LINUX_KCONFIG_SET += CONFIG_SYSTEM_TRUSTED_KEYS
CONFIG_SYSTEM_TRUSTED_KEYS_VALUE =
endif # BR2_CONFIG_UBIFS_FS_AUTHENTICATION

endif # BR2_CONFIG_SECURE_DM_MAPPER


ifeq ($(BR2_CONFIG_FULL_DISK_ENCRYPTION),y)
LINUX_KCONFIG_ENABLE += CONFIG_DM_CRYPT
else
LINUX_KCONFIG_DISABLE += CONFIG_DM_CRYPT
endif

ifeq ($(BR2_CONFIG_FILE_BASED_ENCRYPTION),y)
LINUX_KCONFIG_ENABLE += CONFIG_FS_ENCRYPTION
LINUX_KCONFIG_ENABLE += CONFIG_CRYPTO_CTS
LINUX_KCONFIG_ENABLE += CONFIG_CRYPTO_XTS
else
LINUX_KCONFIG_DISABLE += CONFIG_FS_ENCRYPTION
endif


define LINUX_KCONFIG_CUSTOM_FIXUP
	$(foreach k,$(LINUX_KCONFIG_ENABLE),\
		$(call KCONFIG_ENABLE_OPT,$(k),$(LINUX_DIR)/.config)$(sep))
	$(foreach k,$(LINUX_KCONFIG_DISABLE),\
		$(call KCONFIG_DISABLE_OPT,$(k),$(LINUX_DIR)/.config)$(sep))
	$(foreach k,$(LINUX_KCONFIG_SET),\
		$(call KCONFIG_SET_OPT,$(k),"$($(k)_VALUE)",$(LINUX_DIR)/.config)$(sep))
endef

LINUX_PRE_BUILD_HOOKS += LINUX_KCONFIG_CUSTOM_FIXUP

define LINUX_INSTALL_HEADERS
	$(MAKE1) -C $(BUILD_DIR)/linux-custom headers_install
endef
LINUX_POST_INSTALL_IMAGES_HOOKS += LINUX_INSTALL_HEADERS

ifeq ($(BR2_PACKAGE_CRDA),y)
	LINUX_DEPENDENCIES += crda
endif


# Support for rebuilding the kernel after the root archive has
# been generated.

ifeq ($(BR2_CONFIG_DM_INIT),y)
# squashfs
ifeq ($(BR2_TARGET_ROOTFS_SQUASHFS),y)
ROOTFS_SECURE_ROOT_DEPS = rootfs-squashfs
endif

# ext2
ifeq ($(BR2_TARGET_ROOTFS_EXT2),y)
ROOTFS_SECURE_ROOT_DEPS = rootfs-ext2
endif

.PHONY: linux-rebuild-with-secure-root
linux-rebuild-with-secure-root: $(LINUX_DIR)/.stamp_target_installed
linux-rebuild-with-secure-root: $(LINUX_DIR)/.stamp_images_installed
linux-rebuild-with-secure-root: $(ROOTFS_SECURE_ROOT_DEPS)
linux-rebuild-with-secure-root:
	@$(call MESSAGE,"Rebuilding kernel with crypto or secure root")
	# Build the kernel.
	$(call KCONFIG_ENABLE_OPT,CONFIG_ATAGS,$(LINUX_DIR)/.config)
	$(call KCONFIG_SET_OPT,CONFIG_CMDLINE,"$(shell cat $(BINARIES_DIR)/rootfs_dm_init.txt)",$(LINUX_DIR)/.config)
	$(call KCONFIG_ENABLE_OPT,CONFIG_CMDLINE_EXTEND,$(LINUX_DIR)/.config)
	$(LINUX_MAKE_ENV) $(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) $(LINUX_TARGET_NAME)
	$(LINUX_APPEND_DTB)
	# Copy the kernel image(s) to its(their) final destination
	$(call LINUX_INSTALL_IMAGE,$(BINARIES_DIR))
	# If there is a .ub file copy it to the final destination
	test ! -f $(LINUX_IMAGE_PATH).ub || cp $(LINUX_IMAGE_PATH).ub $(BINARIES_DIR)

rootfs-secureroot: linux-rebuild-with-secure-root

rootfs-secureroot-show-depends:
	@echo $(ROOTFS_SECURE_ROOT_DEPS)

.PHONY: rootfs-secureroot rootfs-secureroot-show-depends

TARGETS_ROOTFS += rootfs-secureroot
endif # BR2_CONFIG_DM_INIT
