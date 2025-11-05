#!/bin/sh


source /etc/profile

ulimit -s 256

echo 1 >/proc/sys/vm/overcommit_memory
echo 5 >/proc/sys/vm/dirty_background_ratio
echo 10 >/proc/sys/vm/dirty_ratio
echo 50 >/proc/sys/vm/dirty_writeback_centisecs
echo 100 >/proc/sys/vm/dirty_expire_centisecs
echo 10000 >/proc/sys/vm/vfs_cache_pressure
echo 1 >/proc/sys/kernel/panic_on_oops


echo 100 > /proc/sys/vm/swappiness
echo 8388608 > /sys/block/zram0/disksize
mkswap /dev/zram0
swapon /dev/zram0



#drivers insmod
insmod /app/drivers/ipc-gpio.ko
insmod /app/drivers/m433_driver.ko

#/app/rt/load.sh

otp_mfg -r --pg --loglv=3

sleep 3

echo 9 >/proc/sys/kernel/printk

dmsetup info -c

cp /app/bin/daemon /tmp/
chmod +x /tmp/daemon
/tmp/daemon @ipc

