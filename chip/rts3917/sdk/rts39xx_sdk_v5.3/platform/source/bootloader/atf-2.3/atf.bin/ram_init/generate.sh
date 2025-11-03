#!/bin/sh

DIRECTORY="./bl2"

if [ -d "$DIRECTORY" ]; then

	if grep -wq "CONFIG_UBOOT_AS_BL33" ./package/boot/uboot/.config; then
		cd ram_init

		cp ../../uboot-custom/ram_init/u-boot_s1.bin ./
		cp ../build/sheipa/release/bl2.bin ./

		filesize=`stat -c %s bl2.bin`

		echo "bl2 filesize is $filesize"
		echo "uboot as bl33"
		dd bs=1 count=$filesize if=bl2.bin of=u-boot_s1.bin skip=0 seek=8192

		cp u-boot_s1.bin ../build/sheipa/release/bl2.bin

	else
		cd ram_init

		echo "uboot as bl2"

		cp ../../uboot-custom/u-boot.bin ./

		output_file="u-boot.bin"

		filesize=`stat -c %s $output_file`

		echo "$output_file size:$filesize"

		if [ `expr $filesize % 4` -ne 0 ];then
			sizea=`expr $filesize / 4`
			sizeb=`expr $sizea \* 4`
			newsize=`expr $sizeb + 4`
			tr '\000' '\377' < /dev/zero| dd of=binfile bs=1 count=$newsize
			dd conv=notrunc if=$output_file of=binfile
			dd conv=notrunc if=binfile of=$output_file
		fi

		echo "$output_file align size is `stat -c %s $output_file`"

		cp u-boot.bin ../build/sheipa/release/bl2.bin
	fi
else
	cd ram_init

	echo "uboot as bl2"

	cp ../../uboot-custom/u-boot.bin ./

	output_file="u-boot.bin"

	filesize=`stat -c %s $output_file`

	echo "$output_file size:$filesize"

	if [ `expr $filesize % 4` -ne 0 ];then
		sizea=`expr $filesize / 4`
		sizeb=`expr $sizea \* 4`
		newsize=`expr $sizeb + 4`
		tr '\000' '\377' < /dev/zero| dd of=binfile bs=1 count=$newsize
		dd conv=notrunc if=$output_file of=binfile
		dd conv=notrunc if=binfile of=$output_file
	fi

	echo "$output_file align size is `stat -c %s $output_file`"

	mkdir -p ../build/sheipa/release/.
	cp u-boot.bin ../build/sheipa/release/bl2.bin
	chmod +x ../build/sheipa/release/bl2.bin
	cd -

	tools/cert_create/cert_create -n --tfw-nvctr 31 --ntfw-nvctr 223 --key-alg rsa --rot-key ../../rtskey/verity_key0.key --tb-fw-cert ./build/sheipa/release/tb_fw.crt --tb-fw ./build/sheipa/release/bl2.bin

	tools/fiptool/fiptool create --tb-fw-cert ./build/sheipa/release/tb_fw.crt --tb-fw ./build/sheipa/release/bl2.bin build/sheipa/release/fip.bin

	fip_file="build/sheipa/release/fip.bin"

	filesize=`stat -c %s $fip_file`

	echo "$fip_file size:$filesize"

	if [ `expr $filesize % 4` -ne 0 ];then
		sizea=`expr $filesize / 4`
		sizeb=`expr $sizea \* 4`
		newsize=`expr $sizeb + 4`
		tr '\000' '\377' < /dev/zero| dd of=binfile bs=1 count=$newsize
		dd conv=notrunc if=$fip_file of=binfile
		dd conv=notrunc if=binfile of=$fip_file
	fi

	echo "$fip_file align size is `stat -c %s $fip_file`"
fi
