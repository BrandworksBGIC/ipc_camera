#!/bin/sh
#
# This script at first changes root filesystem to ramfs-based path,
# then unmounts old squashfs-based rootfs and kills all unrelated
# background processes, and finally updates the data stored on flash
#
# chgroot.sh [image]
# for example:
# chgroot.sh /tmp/image
#

stop_prog(){
	if [ "$(ps | grep uvcd | grep -v grep)" ]; then
		killall uvcd
	fi
	if [ "$(ps | grep uacd | grep -v grep)" ]; then
		killall uacd
	fi
	if [ "$(ps | grep ubusd | grep -v grep)" ]; then
		killall ubusd
	fi
	if [ "$(ps | grep syslogd | grep -v grep)" ]; then
		killall syslogd
	fi

	if [ "$(ps | grep klogd | grep -v grep)" ]; then
		killall klogd
	fi

	if [ "$(ps | grep uvca | grep -v grep)" ]; then
		killall uvca
	fi

	if [ "$(ps | grep qrdemo | grep -v grep)" ]; then
		killall qrdemo
	fi

	if [ "$(ps | grep entropy | grep -v grep)" ]; then
		killall entropy
	fi

	if [ "$(ps | grep gaia | grep -v grep)" ]; then
		killall gaia
	fi
	sleep 1
}

chgroot_prog(){
	stop_prog
	configfs.sh stop
	fs_mgr -u /etc/fstab.user
	configfs.sh start D
	#create directory
	mkdir /tmp/root -p
	mount --bind /tmp/root /tmp/root
	cd /tmp/root/
	mkdir -p bin lib sbin sys proc mnt var dev media  overlay mnt/tmp

	#copy binary
	cd /bin
	cp busybox basename dirname ash sh mount umount sync \
		grep cp mv ls cat sleep mkdir rmdir rm kill \
		chmod top ps df killall ln dfud \
		chgroot.sh fw_printenv fw_setenv /tmp/root/bin/ -d

	cd /sbin
	cp init pivot_root chroot reboot devmem /tmp/root/sbin/ -d

	#copy library
	cd /lib
	cp ld-uClibc* libgcc* libuClibc-* libc.* /tmp/root/lib/ -d

	#move mount point
	mount --move /dev /tmp/root/dev/
	mount --move /sys /tmp/root/sys/
	mount --move /var /tmp/root/var/

	cd /tmp/root
	pivot_root . mnt/tmp
	mount --move mnt/tmp/proc/ /proc
	grep /overlay /proc/mounts > /dev/null && {
		/bin/mount -o noatime,nodiratime,remount,ro /mnt/tmp/overlay
		/bin/umount /mnt/tmp/overlay
	}

	cd /
	/bin/mkdir -p /tmp
	sync
	/bin/dfud &
	/bin/killall -9 sh
}

uboot_dfu_enable_done(){
	BOOT_COUNT_REG=$(/bin/fw_printenv boot_count_reg)
	BOOT_COUNT_REG="0x""${BOOT_COUNT_REG##*=}"
	/sbin/devmem $BOOT_COUNT_REG 32 0
	/bin/fw_setenv uboot_dfu_enable 0
}

rauc_prog(){
	stop_prog
	configfs.sh stop
	fs_mgr -u /etc/fstab.user
	configfs.sh start D
}

if [ $# -eq 0 ]; then
	chgroot_prog
elif [ $# -eq 1 ]; then
	case $1 in
                "uboot_dfu_enable_done"):
                        uboot_dfu_enable_done
                        ;;
		"rauc"):
			rauc_prog
			;;
                *):
                        ;;
        esac
fi
