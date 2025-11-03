#!/bin/sh
#
# This script use rauc update fw file
#
# switchroot_rauc.sh [burntool] [image]
# for example:
# switchroot_rauc.sh /tmp/dual_image.raucb
#

echo "$*";

#IMAGE_FILE=$1
IMAGE_BASE=$(basename "$1")
IMAGE_DIR=$(dirname "$1")

echo "dualimage_seta.sh starts..."
#echo "$IMAGE_FILE"

killall mips-linux-lighttpd
killall syslogd klogd
killall fr ubusd nm_init audioplayer bless
killall io wpa_supplicant udhcpc ntpclient
killall dbus-daemon entropy gaia

#create directory
mkdir /tmp/root -p
mv "$1" "/tmp/root/${IMAGE_BASE}"
mount --bind /tmp/root /tmp/root
cd /tmp/root/
mkdir -p bin lib sbin sys proc mnt var dev media/data/ etc/rauc/ usr/www/cgi-bin/ overlay mnt/tmp

#copy binary
cd /bin
cp busybox basename dirname ash sh mount umount sync \
	grep cp mv ls cat sleep  mkdir rmdir rm kill \
	chmod top ps df killall neuralyzer switchroot.sh ln \
	switchroot_rauc.sh rauc fw_printenv fw_setenv\
	expr find vi /tmp/root/bin/ -dv

cd /sbin
cp init pivot_root chroot reboot flash_erase nandwrite /tmp/root/sbin/ -dv

cp pivot_root chroot reboot flash_erase nandwrite /tmp/root/bin/ -dv

#copy library
cd /lib
cp ld-uClibc* libgcc* libuClibc-* libc.* libglib* \
	libjson* libgio* libgobject* libgmodule* \
	libz* libmount* libblkid* libuuid* libffi* \
	libpcre* libintl* libssl* libcrypto* librtsnm* \
	libsysconf* \
	/tmp/root/lib/  -dv

cd /tmp/root/usr
ln -s ../lib lib
ln -s ../bin bin

cd /etc/rauc
cp trunk.* system.conf /tmp/root/etc/rauc -dv

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
mv "${IMAGE_BASE}" "/tmp/dual_image.raucb"

sync

exec chroot . /bin/busybox sh -c '/bin/rauc -d install /tmp/dual_image.raucb; exec reboot -f'
