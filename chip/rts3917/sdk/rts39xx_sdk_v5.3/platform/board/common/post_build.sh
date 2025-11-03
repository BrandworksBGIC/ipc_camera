#/usr/bin/env bash

. $BR2_EXTERNAL_platform_PATH/board/common/inc.sh

mkdir -p $TARGET_DIR/overlay
mkdir -p $TARGET_DIR/rom
mkdir -p $TARGET_DIR/usr/conf
mkdir -p $TARGET_DIR/usr/rodata

# Process common files
[ -f $BR2_EXTERNAL_platform_PATH/board/common/motd ] && \
	cp -fv $BR2_EXTERNAL_platform_PATH/board/common/motd $TARGET_DIR/etc/

[ -f $BR2_EXTERNAL_platform_PATH/source/ipcam/systemw/mdev.conf ] && \
	cp -fv $BR2_EXTERNAL_platform_PATH/source/ipcam/systemw/mdev.conf $TARGET_DIR/etc/

# Purge unneeded files
if [ -n $TARGET_DIR ];then
	showInfo "Purging rootfs ..."
	if [ -d $TARGET_DIR/usr/include ]; then
		rm -fr $TARGET_DIR/usr/include
	fi
	if [ -d $TARGET_DIR/usr/share/aclocal ]; then
		rm -fr $TARGET_DIR/usr/share/aclocal
	fi
	if [ -d $TARGET_DIR/usr/share/man ]; then
		rm -fr $TARGET_DIR/usr/share/man
	fi
	if [ -d $TARGET_DIR/usr/lib/pkgconfig ]; then
		rm -fr $TARGET_DIR/usr/lib/pkgconfig
	fi
fi
