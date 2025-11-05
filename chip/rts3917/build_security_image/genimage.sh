#!/bin/bash
set -e

BASE_DIR=$(readlink -f "$0" | xargs dirname)
TEMPLATE_DIR=${BASE_DIR}/template
RTSKEY_DIR=${BASE_DIR}/public_key
BUILD_DIR=${BASE_DIR}/${gv_chip_name}/build
TOOLS_DIR=${BASE_DIR}/tools
IMAGE_DIR=${BASE_DIR}/${gv_chip_name}/images
RELEASE_DIR=${BASE_DIR}/${gv_chip_name}/release
SHA_DIR=${BASE_DIR}/${gv_chip_name}/sha256
SIG_DIR=${BASE_DIR}/${gv_chip_name}/signature
SIG_FILE_DIR=${BASE_DIR}/${gv_chip_name}/file_to_sign

[ -d ${BUILD_DIR} ] || mkdir -p ${BUILD_DIR}
[ -d ${RELEASE_DIR} ] || mkdir -p ${RELEASE_DIR}
[ -d ${SHA_DIR} ] || mkdir -p ${SHA_DIR}
[ -d ${SIG_FILE_DIR} ] || mkdir -p ${SIG_FILE_DIR}


export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${BASE_DIR}/lib
echo $LD_LIBRARY_PATH


# uboot
gen_pubkey_dtb()
{
	echo "Generate public key dtb"
	echo > ${BUILD_DIR}/dummy_image
	sed "s%TAG_KERNEL_BIN%${BUILD_DIR}/dummy_image%g;s%TAG_LOAD_ADDR%0x00000000%g;s%TAG_ENTRY_ADDR%0x00000000%g" \
		${TEMPLATE_DIR}/uboot_dtb.its.template > ${BUILD_DIR}/linux.its
	dtc -O dtb ${TEMPLATE_DIR}/u-boot_pubkey_empty.dts > ${BUILD_DIR}/u-boot_pubkey.dtb
	${TOOLS_DIR}/mkimage_rts \
		-f ${BUILD_DIR}/linux.its \
		-K ${BUILD_DIR}/u-boot_pubkey.dtb \
		-k ${RTSKEY_DIR} \
		-r ${BUILD_DIR}/dummy.img
	dtc -I dtb ${BUILD_DIR}/u-boot_pubkey.dtb > ${BUILD_DIR}/u-boot_pubkey.dts
}

gen_uboot()
{
	cp ${BUILD_DIR}/tb_fw_no_sig.crt ${BUILD_DIR}/tb_fw.crt
	cat ${SIG_DIR}/uboot.signature >> ${BUILD_DIR}/tb_fw.crt
	echo "Generate fip.bin"
	${TOOLS_DIR}/fiptool create \
		--tb-fw-cert ${BUILD_DIR}/tb_fw.crt \
		--tb-fw ${BUILD_DIR}/bl2.bin \
		${RELEASE_DIR}/fip.bin
}

gen_uboot_sha()
{
	cp ${IMAGE_DIR}/bl2.bin ${BUILD_DIR}/bl2.bin

        echo "Generate bl2 content certificate with fake private key and real public key"
        ${TOOLS_DIR}/cert_create_rts \
                -n \
                --tfw-nvctr 31 \
                --ntfw-nvctr 223 \
                --key-alg rsa \
                --rot-key ${RTSKEY_DIR}/verity_key0_fake.key \
		--rot-public-key ${RTSKEY_DIR}/verity_key0.pem \
                --tb-fw ${BUILD_DIR}/bl2.bin \
                --tb-fw-cert ${BUILD_DIR}/tb_fw_fake.crt
	
	echo "Generate bl2 content certificate without signature"
	cert_size=$(stat -c%s "${BUILD_DIR}/tb_fw_fake.crt")
	no_sig_cert_size=$((cert_size - 256))
	dd if=${BUILD_DIR}/tb_fw_fake.crt of=${BUILD_DIR}/tb_fw_no_sig.crt bs=1 count=$no_sig_cert_size

	echo "Generate bl2 content certificate to be signed"
	sha_bytes=$(dd if=${BUILD_DIR}/tb_fw_fake.crt bs=1 skip=6 count=2 2>/dev/null | xxd -p)
	length=$(echo "ibase=16; $sha_bytes" | bc)
	length=$((length + 4))
	dd if=${BUILD_DIR}/tb_fw_fake.crt of=${SIG_FILE_DIR}/tb_fw_to_sig.crt bs=1 skip=4 count=$length
	${BASE_DIR}/tools/openssl dgst -sha256 -binary -out ${SHA_DIR}/uboot.sha256 ${SIG_FILE_DIR}/tb_fw_to_sig.crt
}


gen_kernel()
{
	echo "Create linux FIT image";
	sed "s%TAG_KERNEL_BIN%${IMAGE_DIR}/zImage%g; s%TAG_LOAD_ADDR%`cat ${IMAGE_DIR}/Makefile.boot | grep "loadaddr" | awk '{ print $3 }'`%g; s%TAG_ENTRY_ADDR%`cat ${IMAGE_DIR}/Makefile.boot | grep "loadaddr" | awk '{ print $3 }'`%g; s%SIG_KERNEL%`${TOOLS_DIR}/get_sig_value.py ${SIG_DIR}/kernel.signature`%g" ${TEMPLATE_DIR}/linux.its.template > ${BUILD_DIR}/linux.its

	${TOOLS_DIR}/mkimage \
			-f ${BUILD_DIR}/linux.its \
			-r ${RELEASE_DIR}/linux.itb
}

gen_kernel_sha()
{
	cp ${IMAGE_DIR}/zImage ${SIG_FILE_DIR}/
	${TOOLS_DIR}/openssl dgst -sha256 -binary -out ${SHA_DIR}/kernel.sha256 ${SIG_FILE_DIR}/zImage
}

# rootfs
gen_rootfs()
{
	echo "Generate signed root filesystem image rootfs.squashfs.signed"

	${TOOLS_DIR}/build_verity_img.py build \
		${TOOLS_DIR} \
		${IMAGE_DIR}/rootfs.squashfs \
		${RELEASE_DIR}/rootfs.squashfs.signed \
		${SIG_DIR}

}

gen_rootfs_sha()
{
        ${TOOLS_DIR}/build_verity_img.py sha \
                ${TOOLS_DIR} \
                ${IMAGE_DIR}/rootfs.squashfs \
                "/newroot" \
                "/dev/mtdblock4" \
                ${RELEASE_DIR}/rootfs.squashfs.signed \
		${SHA_DIR}
	cp ${IMAGE_DIR}/rootfs.squashfs.table ${SIG_FILE_DIR}/
}

# user partition
gen_user()
{
	echo "Generate app.signed"
	${TOOLS_DIR}/build_verity_img.py build \
		${TOOLS_DIR} \
		${IMAGE_DIR}/app.bin \
		${RELEASE_DIR}/app.bin.signed \
		${SIG_DIR}
}

gen_user_sha()
{
        echo "Generate app.signed.sha"
        ${TOOLS_DIR}/build_verity_img.py sha \
                ${TOOLS_DIR} \
                ${IMAGE_DIR}/app.bin \
                "/app" \
                "/dev/mtdblock5" \
                ${RELEASE_DIR}/app.bin.signed \
                ${SHA_DIR}
	cp ${IMAGE_DIR}/app.bin.table ${SIG_FILE_DIR}/
}

build_image(){
			gen_uboot
# 		gen_dtb
		gen_kernel
		gen_rootfs
		gen_user
}

get_sha(){
		gen_pubkey_dtb
		gen_uboot_sha
#   gen_dtb_sha
	gen_kernel_sha
	gen_rootfs_sha
	gen_user_sha
}

print_usage(){
        echo "usage: $PN [build | sha]"
}

PN="$0"

if [ $# -eq 0 ]; then
        build_image
elif [ $# -eq 1 ]; then
        case $1 in
                "build"):
                        #echo start
                        build_image
                        ;;
                "sha"):
                        #echo stop
                        get_sha
                        ;;
                *):
                        print_usage
                        ;;
        esac
else
        print_usage
fi
