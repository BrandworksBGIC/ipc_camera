#!/bin/bash
set -e

SCRIPT_DIR=$(readlink -f "$0" | xargs dirname)
BASE_DIR=${SCRIPT_DIR}
RTSKEY_DIR=${BASE_DIR}/private_key
TOOLS_DIR=${BASE_DIR}/tools
IMAGE_DIR=${BASE_DIR}/${gv_chip_name}/images
SHA_DIR=${BASE_DIR}/${gv_chip_name}/sha256
SIG_DIR=${BASE_DIR}/${gv_chip_name}/signature
SIG_FILE_DIR=${BASE_DIR}/${gv_chip_name}/file_to_sign

[ -d ${SIG_DIR} ] || mkdir -p ${SIG_DIR}

# Check if file needs update
file_needs_update() {
    local src_file="$1"
    local target_file="$2"

    if [[ ! -f "$target_file" ]]; then
        return 0  # Target file doesn't exist, need to create
    fi

    [[ "$src_file" -nt "$target_file" ]]
}

# uboot
gen_uboot_sign()
{
	if file_needs_update "${SHA_DIR}/uboot.sha256" "${SIG_DIR}/uboot.signature"; then
		echo "🔐 Generating uboot signature..."
		#${TOOLS_DIR}/sign_uboot ${SIG_FILE_DIR}/tb_fw_to_sig.crt ${RTSKEY_DIR}/verity_key0.key ${SIG_DIR}/uboot.signature
		./sign_partition.sh ${SIG_FILE_DIR}/tb_fw_to_sig.crt  rts3917 rsa_pkcs_pss ${SIG_DIR}/uboot.signature
		echo "✅ uboot signature generated"
	else
		echo "⏭️  uboot signature skipped (no changes)"
	fi
}

gen_dtb_sign()
{
	if file_needs_update "${SHA_DIR}/dtb.sha256" "${SIG_DIR}/dtb.signature"; then
		echo "🔐 Generating dtb signature..."
		#openssl dgst -sha256 -sign ${RTSKEY_DIR}/verity_key1.key -out ${SIG_DIR}/dtb.signature ${IMAGE_DIR}/rts3917n_evb_cp_16m.dtb

		../../../hsm/signEmul/signEmul rts3917_sec_boot ${SHA_DIR}/dtb.sha256 ${SIG_DIR}/dtb.signature
		echo "✅ dtb signature generated"
	else
		echo "⏭️  dtb signature skipped (no changes)"
	fi
}

# kernel
gen_kernel_sign()
{
	if file_needs_update "${SHA_DIR}/kernel.sha256" "${SIG_DIR}/kernel.signature"; then
		echo "🔐 Generating kernel signature..."
		# openssl dgst -sha256 -sign ${RTSKEY_DIR}/verity_key1.key -out ${SIG_DIR}/kernel.signature ${IMAGE_DIR}/zImage

		./sign_partition.sh ${IMAGE_DIR}/zImage  rts3917 rsa_pkcs ${SIG_DIR}/kernel.signature

		echo "✅ kernel signature generated"
	else
		echo "⏭️  kernel signature skipped (no changes)"
	fi
}

# rootfs
gen_rootfs_sign()
{
	if file_needs_update "${IMAGE_DIR}/rootfs.squashfs.table" "${SIG_DIR}/rootfs.squashfs.signature"; then
		echo "🔐 Generating rootfs signature..."
		# openssl dgst -sha256 -sign ${RTSKEY_DIR}/verity_key2.key -out ${SIG_DIR}/rootfs.squashfs.signature ${IMAGE_DIR}/rootfs.squashfs.table

		./sign_partition.sh ${IMAGE_DIR}/rootfs.squashfs.table  rts3917 rsa_pkcs ${SIG_DIR}/rootfs.squashfs.signature

		echo "✅ rootfs signature generated"
	else
		echo "⏭️  rootfs signature skipped (no changes)"
	fi
}

# user partition
gen_user_sign()
{
	if file_needs_update "${IMAGE_DIR}/app.bin.table" "${SIG_DIR}/app.bin.signature"; then
		echo "🔐 Generating app signature..."
		# openssl dgst -sha256 -sign ${RTSKEY_DIR}/verity_key2.key -out ${SIG_DIR}/app.bin.signature ${IMAGE_DIR}/app.bin.table

		./sign_partition.sh ${IMAGE_DIR}/app.bin.table  rts3917 rsa_pkcs ${SIG_DIR}/app.bin.signature

		echo "✅ app signature generated"
	else
		echo "⏭️  app signature skipped (no changes)"
	fi
}

gen_uboot_sign
# gen_dtb_sign
gen_kernel_sign
gen_rootfs_sign
gen_user_sign

echo "genimage signature successfully"

