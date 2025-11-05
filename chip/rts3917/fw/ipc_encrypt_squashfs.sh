#!/bin/bash

sdk_dir=`pwd`/../sdk/rts39xx_sdk_v5.3/

source_image=`pwd`/images

cd ../build_security_image/
./run.sh $source_image $1 $2

exit $?