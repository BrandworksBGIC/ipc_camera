#!/bin/bash

show_usage()
{
	echo
	echo "Usage:"
	echo "$0"
	echo "	   Select one defconfig in interactive way"
	echo "$0 <config_file>"
	echo "	   <config_file>: specify one defconfig file located in \
platform/configs"
	echo "	   Example:"
	echo "	   for rts3903 EVB: $0 rts3903_evb_defconfig"
	echo "$0 <config_file> <outpath>"
	echo "	   <config_file>: specify one defconfig file located in \
platform/configs"
	echo "	   <outpath>: specify the out path name"
	echo "	   Example:"
	echo "	   for rts3903 EVB: $0 rts3903_evb_defconfig rts3903_evb_1"
	echo "$0 --help|-h"
	echo "	   Show this help"
	echo
	exit 0
}

TOPDIR=$(pwd)
EXTERNAL_DIR=$TOPDIR/platform

if [ $# -eq 0 ]; then
	#COLUMNS=0
	PS3="Type a number or 'q' to quit: "
	select DEFCONFIG in \
		$(find ${EXTERNAL_DIR}/configs/ -type f -name "*_defconfig" \
			-exec basename {} \; | sort); do
		if [ $REPLY = q ]; then
			exit
		fi
		if [ -z "$DEFCONFIG" ]; then
			echo "Invalid choice, please try again."
		else
			echo "$DEFCONFIG ($REPLY) is picked to launch."
			break
		fi
	done
	if [ -z "$DEFCONFIG" ]; then
		echo "No defconfigs are found."
		exit 1
	fi

	DEFCONFIG_PREFIX=${DEFCONFIG%%_defconfig}

	read -e -p "Enter the build workspace folder name: " -i \
		$DEFCONFIG_PREFIX WORKPATH
	WORKPATH=${WORKPATH%%/*}
	if [ -z $WORKPATH ];then
		echo "# empty input, use default"
		WORKPATH=$DEFCONFIG_PREFIX
	fi
	echo "#"
	echo "# build workspace path is out/$WORKPATH"
	echo "#"

else
	DEFCONFIG=$1
	WORKPATH=${DEFCONFIG%%_defconfig}
	if [ $DEFCONFIG = --help ] || [ $DEFCONFIG = -h ]; then
		show_usage
	fi
fi


OUT_DIR=$TOPDIR/out/$WORKPATH
BR_DIR=$TOPDIR/buildroot-dist
BR_CONFIG=$EXTERNAL_DIR/configs/$DEFCONFIG
if [ $# -gt 1 ]; then
	OUT_DIR=$TOPDIR/out/$2
fi

if [ -d toolchain ]; then
	TOOLCHAIN_NAME=$(grep BR2_TOOLCHAIN_EXTERNAL_PATH ${EXTERNAL_DIR}/configs/${DEFCONFIG})
	TOOLCHAIN_NAME=${TOOLCHAIN_NAME##*/}
	TOOLCHAIN_NAME=${TOOLCHAIN_NAME%\"*}
	RSDKS=`find -L toolchain -maxdepth 1 -mindepth 1 -name "$TOOLCHAIN_NAME" -print | \
		sed -e 's?^toolchain/??' -e 's?/[^/]*$??' -e 's?/? ?g' | \
		   sort`

	if [ -z "$RSDKS" ]; then
		echo "toolchain $TOOLCHAIN_NAME not found"
		exit 1
	fi
else
	echo "toolchain does not exist"
	exit 1
fi

if [ ! -f $BR_CONFIG ]; then
	echo "$BR_CONFIG does not exist"
	exit 1
fi

mkdir -p $OUT_DIR/suitcase
mkdir -p $OUT_DIR/userdata
cd $BR_DIR && make BR2_EXTERNAL=$EXTERNAL_DIR O=$OUT_DIR $DEFCONFIG

