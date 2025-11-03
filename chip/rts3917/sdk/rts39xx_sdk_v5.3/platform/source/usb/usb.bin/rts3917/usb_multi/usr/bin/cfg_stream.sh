#!/bin/sh

cfg_frame() {
NAME=$1
IDX=$2
FORMAT=$3
WIDTH=$4
HEIGHT=$5
MIN_FPS=$6
MAX_FPS=$7
FPS=$8

fdir=/sys/kernel/config/usb_gadget/rts_multi/functions/$NAME/streaming
if [ $FORMAT -lt 2 ] ; then
wdir=$fdir/uncompressed/u.$IDX/${WIDTH}x${HEIGHT}
elif [ $FORMAT -eq 2 ]; then
wdir=$fdir/mjpeg/m.$IDX/${WIDTH}x${HEIGHT}
elif [ $FORMAT -eq 3 ]; then
wdir=$fdir/h264/h.$IDX/${WIDTH}x${HEIGHT}
elif [ $FORMAT -eq 4 ]; then
wdir=$fdir/h265/hh.$IDX/${WIDTH}x${HEIGHT}
else
	echo "invalid fmt"
        exit -1
fi

mkdir -p $wdir
if [ $FORMAT = 0 ] ; then
        echo NV12 > \
                $fdir/uncompressed/u.$IDX/guidFormat
        echo 12 > \
                $fdir/uncompressed/u.$IDX/bBitsPerPixel
fi

echo $WIDTH > $wdir/wWidth
echo $HEIGHT > $wdir/wHeight
if [ -f $wdir/dwMaxVideoFrameBufferSize ]; then
	if [ $FORMAT -gt 1 ] ; then
		echo $(( $WIDTH * $HEIGHT * 2 / 3)) > $wdir/dwMaxVideoFrameBufferSize
	elif [ $FORMAT = 0 ] ; then
		echo $(( $WIDTH * $HEIGHT * 3 / 2)) > $wdir/dwMaxVideoFrameBufferSize
	else
		echo $(( $WIDTH * $HEIGHT * 2)) > $wdir/dwMaxVideoFrameBufferSize
	fi
fi
#w*h*minfps*16
minrate=`echo "$WIDTH * $HEIGHT * $MIN_FPS * 16"|bc`
echo $(printf "%.0f" $minrate) > $wdir/dwMinBitRate
#w*h*maxfps* 16
maxrate=`echo "$WIDTH * $HEIGHT * $MAX_FPS * 16"|bc`
echo $(printf "%.0f" $maxrate) > $wdir/dwMaxBitRate
#10000000 /maxfps
definv=`echo "scale=0;10000000/$MAX_FPS"|bc`
echo $definv > $wdir/dwDefaultFrameInterval
echo -e $FPS > $wdir/dwFrameInterval
}

NAME=$1
STRM_IDX=$2
FMT=$3
RES=$4
MIN_FPS=$5
MAX_FPS=$6
FPS=$7
S_RES_Y=$8
S_RES_M=$9
S_RES_AVC=${10}
S_RES_HEVC=${11}

cfg_dir=/sys/kernel/config/usb_gadget/rts_multi/functions/$NAME/streaming

if [ ! -z "$S_RES_Y" ]; then
	echo -e $S_RES_Y > $cfg_dir/uncompressed/still_image/wWidth_wHeight
fi
if [ ! -z "$S_RES_M" ]; then
	echo -e $S_RES_M > $cfg_dir/mjpeg/still_image/wWidth_wHeight
fi
if [ ! -z "$S_RES_AVC" ]; then
	echo -e $S_RES_AVC > $cfg_dir/h264/still_image/wWidth_wHeight
fi
if [ ! -z "$S_RES_HEVC" ]; then
	echo -e $S_RES_HEVC > $cfg_dir/h265/still_image/wWidth_wHeight
fi

for var in $(echo ${FMT} | awk '{split($0,arr," "); \
	for(i in arr) print arr[i]}')
do
	j=0
	for num in $(echo ${RES} | awk '{split($0,arr," "); \
		for(i = length(arr); i >= 1; --i) print arr[i]}')
        do
		if [ $j -eq 0 ] ; then
			w=${num}
			let j++
		else
			h=${num}
			#only mjpeg support 848x480
			if [ ${w} -eq 848 -a ${h} -eq 480 -a ${var} -ne 2 ];
			then
                        	let j--
				continue
			fi
			#yuyv&nv12 not support larger than 640x480
			if [ $((${w}*${h})) -gt 307200 -a ${var} -lt 2 ]; then
				let j--
				continue
			fi
			cfg_frame $NAME $STRM_IDX ${var} ${w} ${h} \
				$MIN_FPS $MAX_FPS $FPS
                        let j--
		fi
        done
	if [ ${var} -lt 2 ] ; then
		ln -s $cfg_dir/uncompressed/u.$STRM_IDX $cfg_dir/header/h
	elif [ ${var} -eq 2 ]; then
		ln -s $cfg_dir/mjpeg/m.$STRM_IDX $cfg_dir/header/h
	elif [ ${var} -eq 3 ]; then
		ln -s $cfg_dir/h264/h.$STRM_IDX $cfg_dir/header/h
	elif [ ${var} -eq 4 ]; then
		ln -s $cfg_dir/h265/hh.$STRM_IDX $cfg_dir/header/h
	else
		echo "invalid fmt ${var}"
		exit -1
	fi
done

