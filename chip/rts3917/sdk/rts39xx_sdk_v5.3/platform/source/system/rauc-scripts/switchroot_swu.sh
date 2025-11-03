#!/bin/sh
#
# This script use swupdate update fw file
#
# switchroot_swu.sh [burntool] [image]
# for example:
# switchroot_swu.sh /tmp/dual_image.swu
#

echo "$*";

IMAGE_FILE=$1

echo "dualimage_seta.sh starts..."
echo "$IMAGE_FILE"

killall mips-linux-lighttpd
killall syslogd klogd
killall rngd telnetd miniupnpd twoway
killall ubusd nm_init peacock bless eventd
killall io wpa_supplicant udhcpc ntpd ntpclient
killall dbus-daemon entropy
killall eagle doorkeeper rtspd gaia

echo 3 > /proc/sys/vm/drop_caches

slot_part=''

B_ORDER=$(/bin/fw_printenv BOOT_ORDER)
if [ "$B_ORDER" ]; then
	B_ORDER=${B_ORDER##*=}
fi
echo "boot order: $B_ORDER"

for x in $(cat /proc/cmdline); do
	if [ "$x" = "rauc.slot=A" ]; then
		echo $x
		slot_part=$x
		flash_erase /dev/mtd5 0x0 0x0
		#reg_addr=$regB_addr
		break
	elif  [ "$x" = "rauc.slot=B" ]; then
		echo $x
		slot_part=$x
		flash_erase /dev/mtd3 0x0 0x0
		#reg_addr=$regA_addr
		break
	fi
done

mv "${IMAGE_FILE}" "/tmp/dual_image.swu"

if [ "$slot_part" = "rauc.slot=A" ]; then
	exec sh -c '/bin/swupdate -l 15 -i /tmp/dual_image.swu -k /etc/swupdate/trunk.cert.pem -e "stable,part_B";  if [ $? -eq 0 ]; then `fw_setenv BOOT_ORDER B A`; exec reboot -f; fi'
else
	exec sh -c '/bin/swupdate -l 15 -i /tmp/dual_image.swu -k /etc/swupdate/trunk.cert.pem -e "stable,part_A";  if [ $? -eq 0 ]; then `fw_setenv BOOT_ORDER A B`; exec reboot -f; fi'
fi
