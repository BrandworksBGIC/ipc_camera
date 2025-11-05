#!/bin/bash
set -e

WORK_PATH=$1
BASE_DIR=$(readlink -f "$0" | xargs dirname)
IMAGE_DIR=${BASE_DIR}/${gv_chip_name}/images

help_info()
{
        echo "getdata.sh [options]"
        echo "Get all files that need to be signed."
        echo ""
        echo "Usage: ./getdata.sh <dir>"
        echo "  dir: sdk working directory"
	exit 0
}

[ -n "${WORK_PATH}" ] || help_info

[ -d ${IMAGE_DIR} ] || mkdir -p ${IMAGE_DIR}

# uboot:
echo "get uboot data"
cp -u ${WORK_PATH}/images/bl2.bin ${IMAGE_DIR}/

#dtb:
#echo "get dtb data"
#cp ${WORK_PATH}/images/rts3917n_evb.dtb ${IMAGE_DIR}/

# kernel:
echo "get kernel data"
cp -u ${WORK_PATH}/build/linux-custom/zImage ${IMAGE_DIR}/
cp -u ${WORK_PATH}/build/linux-custom/arch/arm/mach-realtek/Makefile.boot ${IMAGE_DIR}/

# rootfs:
# echo "get rootfs data"
# cp ${WORK_PATH}/images/rootfs.squashfs ${IMAGE_DIR}/

# other:
# echo "get user data"
# cp ${WORK_PATH}/images/userdata.bin ${IMAGE_DIR}/

