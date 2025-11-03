#!/bin/bash

PROG=$(basename $0)
EXT4_NAME="userdata.ext4"
EXT4_CRYPTED="userdata.ext4.crypted"
EXT4_INTEGRITY="userdata.ext4.integrity"
EXT4_INTEGRITY_CRYPTED="userdata.ext4.integrity.crypted"

HMSG()
{
	echo -e "\033[47;30m$1\033[0m"
}

MSG_INFO()
{
	echo -e "\033[0;33m$1\033[0m"	# yellow
}

MSG_WARN()
{
	echo -e "\033[0;31m$1\033[0m"	# red
}

info()
{
	echo "The script is used to generate emmc userdata image as follows:"
	echo "- normal        : ${EXT4_NAME}"
	echo "- crypto        : ${EXT4_CRYPTED}"
	echo "- secure        : ${EXT4_INTEGRITY}"
	echo "- crypto+secure : ${EXT4_INTEGRITY_CRYPTED}"
	echo ""
}

usage()
{
	echo "Usage: ${PROG} <base_path>"
	echo "- base_path: the relative or absolute path to compile sdk"
	echo ""
	echo "e.g."
	echo "$ ../../platform/scripts/${PROG} ."
	echo "or"
	echo "$ platform/scripts/${PROG} out/rts3918n_evb_emmc"
	echo ""
}

check_exist()
{
	for item in $@
	do
		if [ ! -e ${item} ]; then
			echo "No such file or directory: ${item}, exit!"
			exit
		fi
	done
}

cal_img_size()
{
	max_size=16
	dir_size=$(du -s $1 | cut -f 1)

	let img_size=${dir_size}/1024

	if [ ${img_size} -le 10 ]; then
		let img_size=img_size+2
	else
		let img_size=img_size*6/5
	fi

	if [ ${img_size} -ge ${max_size} ]; then
		let img_size=max_size
	fi
}

info

if [ $# != 1 ]; then
	usage
	exit
fi

BASE_PATH=$(readlink -f $1)
IN_DIR=${BASE_PATH}/userdata
OUT_DIR=${BASE_PATH}/userdata_img

MKFS=${BASE_PATH}/host/sbin/mkfs.ext4
PYTHON3=${BASE_PATH}/host/bin/python3
MKCRYPTFS=${BASE_PATH}/../../platform/scripts/mkcryptfs.py
CRYPTO_KEY=${BASE_PATH}/rtskey/crypto_key.bin
INTEGRITYSETUP=${BASE_PATH}/host/sbin/integritysetup

[ -d ${OUT_DIR} ] && rm -rf ${OUT_DIR}/* || mkdir ${OUT_DIR}

check_exist ${IN_DIR} ${OUT_DIR}
check_exist ${MKFS} ${PYTHON3} ${MKCRYPTFS} ${CRYPTO_KEY} ${INTEGRITYSETUP}
cal_img_size ${IN_DIR}

echo "base path  : $BASE_PATH"
echo "input dir  : ${IN_DIR}"
echo "output dir : ${OUT_DIR}"
echo "image size : ${img_size}M"
echo ""


HMSG "========== normal =========="

${MKFS} -b 4096 -d ${IN_DIR} ${OUT_DIR}/${EXT4_NAME} ${img_size}M

MSG_INFO "generate ${EXT4_NAME}"


HMSG "========== crypto =========="

${PYTHON3} ${MKCRYPTFS} ${OUT_DIR}/${EXT4_NAME} \
	${OUT_DIR}/${EXT4_CRYPTED} ${CRYPTO_KEY} 4096

MSG_INFO "generate ${EXT4_CRYPTED}"


HMSG "========== secure =========="

MSG_WARN "Note: need root privilege for dm-integrity"

cp -a ${OUT_DIR}/${EXT4_NAME} ${OUT_DIR}/${EXT4_INTEGRITY}
pid=$$

sudo ${INTEGRITYSETUP} format -q ${OUT_DIR}/${EXT4_INTEGRITY}
sudo ${INTEGRITYSETUP} open ${OUT_DIR}/${EXT4_INTEGRITY} dm-integrity-$pid
sudo ${MKFS} -b 4096 -d ${IN_DIR} /dev/mapper/dm-integrity-$pid
sudo ${INTEGRITYSETUP} close dm-integrity-$pid

MSG_INFO "generate ${EXT4_CRYPTED}"


HMSG "========== crypto + secure =========="

${PYTHON3} ${MKCRYPTFS} ${OUT_DIR}/${EXT4_INTEGRITY} \
	${OUT_DIR}/${EXT4_INTEGRITY_CRYPTED} ${CRYPTO_KEY} 4096

MSG_INFO "generate ${EXT4_INTEGRITY_CRYPTED}"

