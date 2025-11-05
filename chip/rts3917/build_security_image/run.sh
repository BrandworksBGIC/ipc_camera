#!/bin/bash

echo $1 


#\rm release/*

export gv_chip_name=$2
export need_encrypt=$3

[ -d ${gv_chip_name}/images ] || mkdir -p ${gv_chip_name}/images


export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$(pwd)/host_libs


./getdata.sh ../sdk/rts39xx_sdk_v5.3/out/${gv_chip_name}_base/

cp $1/rootfs.bin ${gv_chip_name}/images/rootfs.squashfs -u
cp $1/app.bin ${gv_chip_name}/images/ -u


./genimage${need_encrypt}.sh sha

./gensignature.sh 

./genimage${need_encrypt}.sh build