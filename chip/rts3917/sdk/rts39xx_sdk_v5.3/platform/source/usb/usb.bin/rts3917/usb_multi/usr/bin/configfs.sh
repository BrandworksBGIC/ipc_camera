#!/bin/sh
# config RTSX Multifunction Device

ITEM=$2
CONFIGFS="/sys/kernel/config"
GADGET="$CONFIGFS/usb_gadget/rts_multi"
VID=0x0bda
PID=0x3900
SERIAL="201911080001"
MANUF="Realtek"
PRODUCT="RTSX Multifunction Device"
CFGSTR="RTSX Multifunction Config"
UVC=0x110
FREQUENCY=15000000
PWR=500
self_pwr=0
UDC=`ls /sys/class/udc`

PROTOCOL=0x01
CLASS=0xef
SUBCLASS=0x2
MAXPACKETSIZE0=0x40
USB=0x200
DEVICE=0x0301

STRM_NUM=1
RES="2592x1944,2688x1520,2560x1920,2560x1440,2304x1296,1920x1080,1440x1080,1280x720,800x600,848x480,640x480,640x360,320x240."
FPS="120,60,30,25,24,23,20,16,15,12,11,10,9,8,7,5,3."
# 0: NV12, 1:YUYV, 2:MJPEG, 3:H264, 4:H265(only uvc1.1 support)
FMT="1,2,3,4."
<<COMMENT
CT ctrl:0x200e
bit1:	Auto-Exposure Mode
bit2:	Auto-Exposure Priority
bit3:	Exposure Time (Absolute)
bit13:	Roll (Absolute)
COMMENT
CAMCTRL="14\n32\n0"
<<COMMENT
UNIT_ID:1:CT,2:PU,3:EU,4:XU,5:Customer XU,6:Mircosoft XU, 7:OT
PU ctrl:0x147f
bit0:	Brightness
bit1:	Contrast
bit2:	Hue
bit3:	Saturation
bit4:	Sharpness
bit5:	Gamma
bit6:	White Balance Temperature
bit10:	Power Line Frequency
bit12:	White Balance Temperature, Auto
COMMENT
if [ "$UVC" = "0x150" ]; then
	PUCTRL="127\n20\n0"
	XU_BaSrc="3"
else
	PUCTRL="127\n20"
	XU_BaSrc="2"
fi
<<COMMENT
XU ctrl:0x063c
bit2:	get log size
bit3:	dump log
bit4:	set metadata size
bit5:	set metadata
bit9:	CMD_STATUS
bit10:	DATA_IN_OUT

CUSTOM XU ctrl:0x1
bit0:	ADVANCED_ISP

Microsoft XU ctrl:0xc00
bit9:	DIGITALWINDOW
bit10:	DIGITALWINDOW_CONFIG
COMMENT
XUCTRL="60\n6"
XU_CTRL_NUM="6"
CUSXUCTRL="1\n0"
CUSXU_CTRL_NUM="1"
MSCTL_1=0
MSCTL_2=12
MSXU_CTRL_NUM="3"

XU_GUID="\x8C\xA7\x29\x12\xB4\x47\x94\x40\
\xB0\xCE\xDB\x07\x38\x6F\xB9\x38"
CUSXU_GUID="\xF9\x26\x9F\x49\x19\xAA\x77\
\x2B\xD4\xA6\x75\x24\x87\x02\x56\x5F"
MSXU_GUID="\xDC\x95\x3F\x0F\x32\x26\x4E\x4C\
\x92\xC9\xA0\x47\x82\xF4\x3B\xC8"

UMSFILE="/dev/mtdblock5"

KEYBOARD="\x05\x01\x09\x06\xA1\x01\x05\x07\
\x19\xE0\x29\xE7\x15\x00\x25\x01\
\x75\x01\x95\x08\x81\x02\x95\x01\
\x75\x08\x81\x03\x95\x05\x75\x01\
\x05\x08\x19\x01\x29\x05\x91\x02\
\x95\x01\x75\x03\x91\x03\x95\x06\
\x75\x08\x15\x00\x25\x65\x05\x07\
\x19\x00\x29\x65\x81\x00\xC0"

idx=0

STILL_IMAGE_METHOD=2
S_RES_Y="640x480,640x360,320x240."
S_RES_M="2560x1440,2304x1296,1920x1080,1440x1080,1280x720,800x600,848x480,640x480,640x360,320x240."
S_RES_AVC="2560x1440,2304x1296,1920x1080,1440x1080,1280x720,800x600,640x480,640x360,320x240."
S_RES_HEVC="2560x1440,2304x1296,1920x1080,1440x1080,1280x720,800x600,640x480,640x360,320x240."

cfg_uvc() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
<<COMMENT
	echo "RTSX UVC 1.5" > functions/$FUNCTION/ictrl
	echo "Video Stream" > functions/$FUNCTION/istrm
	echo "Video Substream" > functions/$FUNCTION/isubstrm
COMMENT
	echo 3072 > functions/$FUNCTION/streaming_maxpacket
	#config control header
	mkdir  functions/$FUNCTION/control/header/h
	echo $UVC > functions/$FUNCTION/control/header/h/bcdUVC
	echo $FREQUENCY > functions/$FUNCTION/control/header/h/dwClockFrequency
	echo 1 > functions/$FUNCTION/control/enable_interrupt_ep
#camera terminal
	echo -e $CAMCTRL > \
		functions/$FUNCTION/control/terminal/camera/default/bmControls
#processing unit
	echo -e $PUCTRL > functions/$FUNCTION/control/processing/default/bmControls
	echo 0 > functions/$FUNCTION/control/processing/default/wMaxMultiplier
#extension unit
	mkdir functions/$FUNCTION/control/extensions/default
	echo 6 > functions/$FUNCTION/control/extensions/default/bNumControls
	echo -e $XUCTRL > functions/$FUNCTION/control/extensions/default/bmControls
	echo -e $XU_BaSrc > functions/$FUNCTION/control/extensions/default/baSourceID
	echo -en $XU_GUID > functions/$FUNCTION/control/extensions/default/guidExtensionCode
#custom extension unit
	mkdir functions/$FUNCTION/control/extensions/custom
	echo 1 > functions/$FUNCTION/control/extensions/custom/bNumControls
	echo -e $CUSXUCTRL > functions/$FUNCTION/control/extensions/custom/bmControls
	echo 4 > functions/$FUNCTION/control/extensions/custom/baSourceID
	echo -en $CUSXU_GUID > functions/$FUNCTION/control/extensions/custom/guidExtensionCode
#mircosoft extension unit
	mkdir functions/$FUNCTION/control/extensions/ms
	echo 3 > functions/$FUNCTION/control/extensions/ms/bNumControls
	#echo -e $MSXUCTRL > functions/$FUNCTION/control/extensions/ms/bmControls
	echo 5 > functions/$FUNCTION/control/extensions/ms/baSourceID
	echo -en $MSXU_GUID > functions/$FUNCTION/control/extensions/ms/guidExtensionCode
#cfg stream
	mkdir functions/$FUNCTION/streaming/header/h
	echo $STILL_IMAGE_METHOD > \
		functions/$FUNCTION/streaming/header/h/bStillCaptureMethod
#cfg stream0 format
	cfg_strm -p $FUNCTION -w $STRM_NUM -r $RES -f $FPS -m $FMT -y $S_RES_Y \
		-j $S_RES_M -h $S_RES_AVC -e $S_RES_HEVC -a 2 640 480
	ret=$?
	if [ $ret -lt 0 ]; then
		echo "config stream fail"
		exit 1
	elif [ $ret -eq 1 ]; then
		MSCTL_1=$((0xf3 & $MSCTL_1))
	fi
	MSXUCTRL="$MSCTL_1\n$MSCTL_2"
	echo -e $MSXUCTRL > functions/$FUNCTION/control/extensions/ms/bmControls
	ln -s functions/$FUNCTION/streaming/header/h \
		functions/$FUNCTION/streaming/class/fs
	ln -s functions/$FUNCTION/streaming/header/h \
		functions/$FUNCTION/streaming/class/hs
	ln -s functions/$FUNCTION/streaming/header/h \
		functions/$FUNCTION/streaming/class/ss
#cfg control header descriptor as fs and ss
	ln -s functions/$FUNCTION/control/header/h  \
		functions/$FUNCTION/control/class/fs
	ln -s functions/$FUNCTION/control/header/h  \
		functions/$FUNCTION/control/class/ss
#bind config with function,do uvc_alloc in f_uvc.c
	ln -s functions/$FUNCTION/ $CONFIG
}

cfg_uac() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
<<COMMENT
default value, no need to change
	echo 0x3 > functions/$FUNCTION/c_chmask
	echo 0x2 > functions/$FUNCTION/c_ssize
	echo 16000 > functions/$FUNCTION/c_srate
	echo 0x3 > functions/$FUNCTION/p_chmask
	echo 0x2 > functions/$FUNCTION/p_ssize
	echo 16000 > functions/$FUNCTION/p_srate
	echo "RTSX UAC 1.0" > functions/$FUNCTION/iassoc
COMMENT
	ln -s functions/$FUNCTION/ $CONFIG
}

cfg_acm() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
<<COMMENT
	echo "CDC Abstract Control Model (ACM)" > functions/$FUNCTION/ctrl
	echo "CDC ACM Data" > functions/$FUNCTION/data
	echo "CDC Serial" > functions/$FUNCTION/iad
cfg port num
	echo 0 > functions/$FUNCTION/port_num
COMMENT
	ln -s functions/$FUNCTION/ $CONFIG
}

cfg_ums() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
#	echo "Mass Storage" > functions/$FUNCTION/ifsg
	echo $UMSFILE > functions/$FUNCTION/lun.0/file
	ln -s functions/$FUNCTION/ $CONFIG
}

cfg_rndis() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
<<COMMENT
	echo "RNDIS Communications Control" > functions/$FUNCTION/ictrl
	echo "RNDIS Ethernet Data" > functions/$FUNCTION/idata
	echo "RNDIS" > functions/$FUNCTION/iad
COMMENT
	ln -s functions/$FUNCTION/ $CONFIG
}

cfg_hid() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
#No Subclass
	echo 0 > functions/$FUNCTION/subclass
#cfg as keyboard
	echo 1 > functions/$FUNCTION/protocol
#report length
	echo 8 > functions/$FUNCTION/report_length
#report desc
	echo -ne $KEYBOARD > functions/$FUNCTION/report_desc
#	echo "HID Interface" > functions/$FUNCTION/istr
	ln -s functions/$FUNCTION/ $CONFIG
}

cfg_dfu() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
#	echo "Device Firmware Upgrade" > functions/$FUNCTION/iname
	ln -s functions/$FUNCTION/ $CONFIG
}

cfg_ecm() {
	CONFIG=$1
	FUNCTION=$2

	mkdir functions/$FUNCTION
	ln -s functions/$FUNCTION/ $CONFIG
}

delete_uvc() {
	CONFIG=$1
	FUNCTION=$2
	HEAD=functions/$FUNCTION/streaming/header/h
	STREAMING=functions/$FUNCTION/streaming
	FORMAT="mjpeg uncompressed h264 h265"

	echo "	Deleting gadget functionality : $FUNCTION"
	rm $CONFIG/$FUNCTION

	rm functions/$FUNCTION/control/class/*/h
	rmdir functions/$FUNCTION/control/header/h
	rmdir functions/$FUNCTION/control/extensions/*

	rm functions/$FUNCTION/streaming/class/*/h
	for file in `ls $HEAD`
	do
		if [ -L $HEAD/$file ] ; then
			rm $HEAD/$file
		fi
	done
	rmdir $HEAD

	for fmt in $FORMAT
	do
		if [ -d $STREAMING/$fmt ] ; then
			for strm in `ls $STREAMING/$fmt`
			do
				if [ $strm != "still_image" ]; then
					for file in `ls $STREAMING/$fmt/$strm`
					do
						if [ -d $STREAMING/$fmt/$strm/$file ] ; then
							rmdir $STREAMING/$fmt/$strm/$file
						fi
					done
					rmdir $STREAMING/$fmt/$strm
				fi
			done
		fi
	done
	rmdir functions/$FUNCTION
}

delete_func() {
	CONFIG=$1
	FUNCTION=$2

	echo "	Deleting gadget functionality : $FUNCTION"
	rm $CONFIG/$FUNCTION

	rmdir functions/$FUNCTION
}

get_version() {
	read -r STR < /etc/version
	numbers=$(echo $STR | grep -o '[0-9]\+')
	set -- $numbers
	VER1=$1
	VER2=$2
	DEVICE=$(printf '0x%02x%02x' "${VER1:-0}" "${VER2:-0}")
}

case "$1" in
    start)
	get_version
	if [ ! -d $CONFIGFS/usb_gadget ]; then
		mount -t configfs none $CONFIGFS
	fi
	mkdir -p $GADGET

	cd $GADGET
	if [ $? -ne 0 ]; then
	    echo "Error creating usb gadget in configfs"
	    exit 1;
	fi

	echo $VID > idVendor
	echo $PID > idProduct

	echo $PROTOCOL > bDeviceProtocol
#USB_CLASS_MISC
	echo $CLASS > bDeviceClass
#USB_CALSS_COMM
	echo $SUBCLASS > bDeviceSubClass
#ep0 max packet size, for usb2.0,always be 64 bytes
	echo $MAXPACKETSIZE0 > bMaxPacketSize0
#for gadget high speed is usb 2.0
	echo $USB > bcdUSB
	echo $DEVICE > bcdDevice
#config strings
	mkdir -p strings/0x409
	echo $SERIAL > strings/0x409/serialnumber
	echo $MANUF > strings/0x409/manufacturer
	echo $PRODUCT > strings/0x409/product
#config configuration
	mkdir configs/c.1
	echo $PWR > configs/c.1/MaxPower
	if [ $self_pwr -ne 0 ]; then
		echo 0xc0 > configs/c.1/bmAttributes
	fi
	mkdir configs/c.1/strings/0x409
	echo $CFGSTR > configs/c.1/strings/0x409/configuration

	echo "$ITEM" |grep -q "R"
	if [ $? -eq 0 ]; then
		cfg_rndis configs/c.1 rndis.usb$idx
		let idx++
	fi

	echo "$ITEM" |grep -q "E"
	if [ $? -eq 0 ]; then
		cfg_ecm configs/c.1 ecm.usb$idx
		let idx++
	fi
	echo "$ITEM" |grep -q "V"
	if [ $? -eq 0 ]; then
		cfg_uvc configs/c.1 uvc.usb$idx
		let idx++
	fi

	echo "$ITEM" |grep -q "A"
	if [ $? -eq 0 ]; then
		cfg_uac configs/c.1 uac1.usb$idx
		let idx++
	fi

	echo "$ITEM" |grep -q "C"
	if [ $? -eq 0 ]; then
		cfg_acm configs/c.1 acm.usb$idx
		let idx++
	fi

	echo "$ITEM" |grep -q "M"
	if [ $? -eq 0 ]; then
		cfg_ums configs/c.1 mass_storage.usb$idx
		let idx++
	fi

	echo "$ITEM" |grep -q "H"
	if [ $? -eq 0 ]; then
		cfg_hid configs/c.1 hid.usb$idx
	fi

	#DFU must be the last one to config
	echo "$ITEM" |grep -q "D"
	if [ $? -eq 0 ]; then
		cfg_dfu configs/c.1 dfu.usb$idx
		let idx++
	fi

	echo $UDC > UDC
	;;

    stop)
	echo "Stopping the USB gadget"

	#set +e # Ignore all errors here on a best effort

	cd $GADGET

	if [ $? -ne 0 ]; then
	    echo "Error: no configfs gadget found"
	    exit 1;
	fi

	echo  > UDC

	for file in `ls functions`
	do
		echo "$file" |grep -q "uvc"
		if [ $? -eq 0 ]; then
			delete_uvc configs/c.1 $file
		else
			delete_func configs/c.1 $file
		fi
	done

	rmdir strings/0x409

	rmdir configs/c.1/strings/0x409
	rmdir configs/c.1

	cd ..
	rmdir rts_multi
	cd /

	;;
    *)
	echo "Usage : $0 {start|stop}"
esac
