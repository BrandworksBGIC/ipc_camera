#!/bin/sh

cd decompress
cp ../u-boot.bin ./
#rm -f ../lib/lzma/*.o
#rm ../common/dlmalloc.o
chmod +x 64bit-lzma

./64bit-lzma e u-boot.bin u-boot.bin.lzma

filesize=`stat -c %s u-boot.bin.lzma`

echo "ram version filesize is $filesize"

decompress_function_size=0x6000
decompress_function_base=0x82800000
touch decompress_lzma.c
$MAKE RAM_VERSION_SIZE_COMP=$filesize \
	DELZMA_FUNC_SIZE=$decompress_function_size \
	DELZMA_FUNC_BASE=$decompress_function_base

#dd bs=1 count=$filesize if=u-boot.bin of=init.bin skip=0 seek=20480
dd bs=1 count=$filesize if=u-boot.bin.lzma of=decompress.bin skip=0 seek=24576
#cp decompress.bin ../../../image/

#./merge.sh
