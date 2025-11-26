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
echo 15728640 > /sys/block/zram0/disksize
mkswap /dev/zram0
swapon /dev/zram0



#drivers insmod
insmod /app/drivers/ipc-gpio.ko
insmod /app/drivers/m433_driver.ko

#/app/rt/load.sh

otp_mfg --ipc_verify
if [ $? -ne 0 ]; then
    echo start write otp
    otp_mfg --ipc_write
    otp_mfg --ipc_lock

    otp_mfg --ipc_verify
    if [ $? -ne 0 ]; then
        echo "OTP verification failed, aborting..."
    fi
    reboot -f
fi


otp_mfg -r --pg --loglv=3

sleep 3

echo 0 >/proc/sys/kernel/printk

dmsetup info -c

cp /app/bin/daemon /tmp/
chmod +x /tmp/daemon
/tmp/daemon @ipc

