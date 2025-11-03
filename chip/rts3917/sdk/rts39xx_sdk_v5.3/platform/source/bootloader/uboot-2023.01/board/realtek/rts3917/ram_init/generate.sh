#!/bin/sh
if grep -wq "CONFIG_SELF_EXTRACTING" include/generated/autoconf.h;
then
cd ram_init
echo "compressed uboot version"
cp ../decompress/decompress.bin ./
filesize=`stat -c %s decompress.bin`

echo "compressed u-boot filesize is $filesize"
initsize=0x4000

touch init.S spi_nand.S
$MAKE -f makefile_raminit RAM_VERSION_SIZE=$filesize \
FLASH_VERSION_SIZE=$initsize DECOMPRESS=$do_decompress

raminitsize=`stat -c %s init.bin`
echo " u-boot 2nd stage size is $raminitsize"

$MAKE -f makefile_boot BOOT_SIZE=2048 RAMINIT_SIZE=$raminitsize

dd bs=1 count=$raminitsize if=init.bin of=sysboot.bin skip=0 seek=2048
mv sysboot.bin init.bin

#dd bs=1 count=$filesize if=u-boot.bin of=init.bin skip=0 seek=20480
dd bs=1 count=$filesize if=decompress.bin of=init.bin skip=0 seek=16384

else
echo "uncompress uboot version"
cd ram_init
cp ../u-boot.bin ./
filesize=`stat -c %s u-boot.bin`
#filesize=262144
initsize=0x4000
touch init.S spi_nand.S
echo "ram version filesize is $filesize"
if grep -wq "CONFIG_RTS_NAND_BOOT" ../include/generated/autoconf.h;
then
#ram uboot limit is 496K
file_limit=507904
else
#ram uboot limit is 376K
file_limit=385024
fi

if [ "$filesize" -gt "$file_limit" ];then
echo "uboot exceed maximum size"
exit 1
fi
$MAKE -f makefile_raminit RAM_VERSION_SIZE=$filesize SYSTEM_INIT_SIZE=$initsize
raminitsize=`stat -c %s init.bin`
echo " u-boot 2nd stage size is $raminitsize"

$MAKE -f makefile_boot BOOT_SIZE=2048 RAMINIT_SIZE=$raminitsize
dd bs=1 count=$raminitsize if=init.bin of=sysboot.bin skip=0 seek=2048

mv sysboot.bin init.bin
cp u-boot.bin u-boot_s2.bin
cp init.bin u-boot_s1.bin
dd bs=1 count=$filesize if=u-boot.bin of=init.bin skip=0 seek=16384
fi
final_filesize=`stat -c %s init.bin`
echo "init.bin size:$final_filesize"

if [ `expr $final_filesize % 4` -ne 0 ];then
        sizea=`expr $final_filesize / 4`
        sizeb=`expr $sizea \* 4`
        newsize=`expr $sizeb + 4`
        tr '\000' '\377' < /dev/zero| dd of=binfile bs=1 count=$newsize
        dd conv=notrunc if=init.bin of=binfile
        cp binfile ../u-boot.bin
else
        cp init.bin ../u-boot.bin
fi

if grep -wq "UBOOT_AS_BL33" ../include/generated/autoconf.h;
then
echo "uboot as bl33"
cp u-boot_s2.bin ../u-boot.bin
fi
#./merge.sh
