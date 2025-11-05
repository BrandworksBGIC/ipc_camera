#!/bin/bash

image_out_dir=./images
rm -f $image_out_dir/rootfs.bin

#rootfsdir=rootfs/rootfs-uclibc-1.1
rootfsdir=rootfs/rootfs_ipcrt
tmp_rootfsdir=/dev/shm/rootfs

rm -rf $tmp_rootfsdir
cp $rootfsdir $tmp_rootfsdir -R -P

touch -ht 202201010800.00 `find $tmp_rootfsdir`
mksquashfs $tmp_rootfsdir $image_out_dir/rootfs.bin -b 64K -comp xz -fstime 1640995200 -all-root
ls -lh $image_out_dir/rootfs.bin
