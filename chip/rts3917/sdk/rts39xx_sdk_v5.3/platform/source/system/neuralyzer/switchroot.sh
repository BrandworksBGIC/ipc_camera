#!/bin/sh
#
# This script at first changes root filesystem to ramfs-based path,
# then unmounts old squashfs-based rootfs and kills all unrelated
# background processes, and finally updates the data stored on flash
#
# switchroot.sh [burntool] [image]
# for example:
# switchroot.sh neuralyzer /tmp/image
#

IMAGE_BASE=$(basename "$2")
IMAGE_DIR=$(dirname "$2")


echo "switchroot.sh starts..."

reg_addr=$(/bin/fw_printenv dual_image_reg_addr)
reg_addr="${reg_addr##*=}"
LEFT=`devmem  $reg_addr`
let A_LEFT=$LEFT%16
let B_LEFT=$LEFT/16

for x in $(cat /proc/cmdline); do
	if [ "$x" = "rauc.slot=A" ]; then
		echo $x
		let value=64+$A_LEFT
		/sbin/devmem $reg_addr 32 $value
		break
	elif  [ "$x" = "rauc.slot=B" ]; then
		echo $x
		fw_setenv BOOT_ORDER A B
		let value=64+$A_LEFT
		/sbin/devmem $reg_addr 32 $value
		break
	fi
done

killall mips-linux-lighttpd
killall syslogd klogd

#kill all background processes in firmware.cgi
#killall -9 entropy nm_init nm_wps ntpclient lark \
	#parrot udhcpd miniupnpd \
	#sync_osd_time doorkeeper event_monitor \
	#tuning-server telnetd wpa_supplicant hostapd \
	#autoipd udhcpc dnrd cam_finder \
	#peacock rtspd gen_aes_shm


#create directory
mkdir /tmp/root -p
mv "$2" "/tmp/root/${IMAGE_BASE}"
mount --bind /tmp/root /tmp/root
cd /tmp/root/
mkdir -p bin lib sbin sys proc mnt var dev media usr/www/cgi-bin/ overlay mnt/tmp

#copy binary
cd /bin
cp busybox basename dirname ash sh mount umount sync \
	grep cp mv ls cat sleep  mkdir rmdir rm kill \
	chmod top ps df killall neuralyzer switchroot.sh ln \
	fw_printenv fw_setenv /tmp/root/bin/ -dv

cd /sbin
cp init pivot_root chroot reboot /tmp/root/sbin/ -dv

#copy library
cd /lib
cp ld-uClibc* libgcc* libuClibc-* libc.* \
	libjson* \
	/tmp/root/lib/  -dv

cp /usr/www/cgi-bin/firmware.cgi /tmp/root/usr/www/cgi-bin/

cd /tmp/root/usr
ln -s ../lib lib
ln -s ../bin bin
ln -s ../sbin sbin

#move mount point
mount --move /dev  /tmp/root/dev/
mount --move /sys  /tmp/root/sys/
mount --move /var  /tmp/root/var/

cd /tmp/root
pivot_root . mnt/tmp
mount --move mnt/tmp/proc/ /proc
grep /overlay /proc/mounts > /dev/null && {
       /bin/mount -o noatime,nodiratime,remount,ro /mnt/tmp/overlay
       /bin/umount /mnt/tmp/overlay
}

cd /
mkdir -p /tmp
mv "/${IMAGE_BASE}" "/tmp/image"

sync

#start upgrade image
echo "start exec /bin/neuralyzer /tmp/image"
exec chroot . /bin/busybox sh -c '/bin/neuralyzer /tmp/image; rm /tmp/image; exec reboot -f'
