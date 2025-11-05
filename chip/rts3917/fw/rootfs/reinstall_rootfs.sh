#!/bin/bash

busybox_root=../../rts39xx_sdk_v5.3/out/rts3917n_base/build/busybox-1.37.0

PATH=$PATH:$(pwd)/../../rts39xx_sdk_v5.3/toolchain/asdk-12.4.1-a7-EL-6.6-u1.0-a32nh-linux-x86_64-250925/bin/

export CROSS_COMPILE=arm-linux-uclibcgnueabi-

\rm $busybox_root/_install -rf

make -C $busybox_root install

source_root=$busybox_root/_install

#rootfs=rootfs-uclibc-1.1
rootfs=rootfs_cprt

if [ -z $rootfs ];then
    exit
fi

find $rootfs/bin -type l -exec \rm {} \;
find $rootfs/sbin -type l -exec \rm {} \;
find $rootfs/usr/bin -type l -exec \rm {} \;
find $rootfs/usr/sbin -type l -exec \rm {} \;

cp $source_root/bin/* $rootfs/bin -r
cp $source_root/sbin/* $rootfs/sbin -r
cp $source_root/usr/bin/* $rootfs/usr/bin -r
cp $source_root/usr/sbin/* $rootfs/usr/sbin -r


# for fsmgr
cd $rootfs/usr/bin
ln -s ../../bin/busybox sh 