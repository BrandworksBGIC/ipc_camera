#/usr/bin/env bash

. $BR2_EXTERNAL_platform_PATH/board/common/inc.sh

function copy_pack_ini ()
{
	if [ ! -e $1/merge.ini ]; then
		cp -v $2 $1/merge.ini
	fi
	if [ ! -e $1/raw.ini ]; then
		cp -v $2 $1/raw.ini
	fi
}

copy_pack_ini $BASE_DIR $BR2_FW_PACK_MERGE_FILE
