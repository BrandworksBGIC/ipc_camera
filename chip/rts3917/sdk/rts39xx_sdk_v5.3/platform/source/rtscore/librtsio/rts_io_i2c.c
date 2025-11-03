#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "rts_io_i2c.h"
#include "rts_io_errno.h"

static struct flock *file_lock(short type, short whence)
{
	static struct flock ret;

	ret.l_type = type;
	ret.l_start = 0;
	ret.l_whence = whence;
	ret.l_len = 0;
	ret.l_pid = getpid();

	return &ret;
}

int rts_io_i2c_write(int i2c_nr, int slave_addr, int addr_type, int count, uint8_t *buf)
{
	int fd;
	int ret;
	char dev[20];
	struct i2c_msg i2cmsg;
	struct i2c_rdwr_ioctl_data msg_rdwr;

	snprintf(dev, sizeof(dev), "/dev/i2c-%d", i2c_nr);
	fd = open(dev, O_WRONLY);
	if (fd < 0) {
		printf("Can't Open '%s'\n", dev);
		return -ERR_IO_I2C_OPENDEV_FAIL;
	}

	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;
	i2cmsg.addr = (uint16_t)slave_addr;
	i2cmsg.flags = 0;
	i2cmsg.len = (uint16_t)count;
	i2cmsg.buf = buf;

	if (addr_type == I2C_ADDR_TEN)
		i2cmsg.flags |= I2C_M_TEN;

	if (fcntl(fd, F_SETLKW, file_lock(F_WRLCK, SEEK_SET)) == -1) {
		close(fd);
		return -1;
	}
	ret = ioctl(fd, I2C_RDWR, &msg_rdwr);
	if (fcntl(fd, F_SETLKW, file_lock(F_UNLCK, SEEK_SET)) == -1)
		ret = -1;
	close(fd);

	if (1 == ret)
		ret = 0;
	else
		ret = -ERR_IO_I2C_IOCTL_FAIL;

	return ret;
}

int rts_io_i2c_read(int i2c_nr, int slave_addr, int addr_type, int count, uint8_t *buf)
{
	int fd;
	int ret;
	char dev[20];
	struct i2c_msg i2cmsg;
	struct i2c_rdwr_ioctl_data msg_rdwr;

	snprintf(dev, sizeof(dev), "/dev/i2c-%d", i2c_nr);
	fd = open(dev, O_RDWR);
	if (fd < 0) {
		printf("Can't Open '%s'\n", dev);
		return -ERR_IO_I2C_OPENDEV_FAIL;
	}

	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;
	i2cmsg.addr = (uint16_t)slave_addr;
	i2cmsg.flags = I2C_M_RD;
	i2cmsg.len = (uint16_t)count;
	i2cmsg.buf = buf;

	if (addr_type == I2C_ADDR_TEN)
		i2cmsg.flags |= I2C_M_TEN;

	if (fcntl(fd, F_SETLKW, file_lock(F_RDLCK, SEEK_SET)) == -1) {
		close(fd);
		return -1;
	}
	ret = ioctl(fd, I2C_RDWR, &msg_rdwr);
	if (fcntl(fd, F_SETLKW, file_lock(F_UNLCK, SEEK_SET)) == -1)
		ret = -1;
	close(fd);

	if (1 == ret)
		ret = 0;
	else
		ret = -ERR_IO_I2C_IOCTL_FAIL;

	return ret;
}

int rts_io_i2c_bulk_read(int i2c_nr, int slave_addr, int addr_type, int reg, int reg_len, int count, uint8_t *val)
{
	int ret;
	int i;
	uint8_t *buf;

	if (reg_len > sizeof(reg))
		return -EINVAL;

	buf = calloc(1, reg_len);
	if (!buf)
		return -ERR_IO_I2C_NOMEM;

	for (i = reg_len -1; i >= 0; i--) {
		buf[i] = (uint8_t)reg;
		reg = reg >> 8;
	}

	ret = rts_io_i2c_write(i2c_nr, slave_addr, addr_type, reg_len, buf);
	free(buf);

	if (ret < 0)
		return ret;

	return rts_io_i2c_read(i2c_nr, slave_addr, addr_type, count, val);
}

int rts_io_i2c_bulk_write(int i2c_nr, int slave_addr, int addr_type, int reg, int reg_len, int count, uint8_t *val)
{
	int ret;
	int i;
	uint8_t *buf;

	if (reg_len > sizeof(reg) || count > MAX_BULK_WRITE_LEN)
		return -EINVAL;

	buf = calloc(1, count + reg_len);
	if (!buf)
		return -ERR_IO_I2C_NOMEM;

	for (i = reg_len -1; i >= 0; i--) {
		buf[i] = (uint8_t)reg;
		reg = reg >> 8;
	}

	memcpy(&(buf[reg_len]), val, count);

	ret = rts_io_i2c_write(i2c_nr, slave_addr, addr_type, count + reg_len, buf);
	free(buf);

	return ret;
}

int rts_io_i2c_byte_read(int i2c_nr, int slave_addr, int addr_type, int reg, int reg_len, uint8_t *val)
{
	return rts_io_i2c_bulk_read(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, val);
}

int rts_io_i2c_byte_write(int i2c_nr, int slave_addr, int addr_type, int reg, int reg_len, uint8_t val)
{
	return rts_io_i2c_bulk_write(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, &val);
}

int rts_io_i2c_set_bits(int i2c_nr, int slave_addr, int addr_type, int reg, int reg_len, uint8_t bit_mask)
{
	uint8_t val;
	int ret;

	ret = rts_io_i2c_bulk_read(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, &val);
	if (ret < 0)
		return ret;

	if ((val & bit_mask) != bit_mask) {
		val |= bit_mask;
		ret = rts_io_i2c_bulk_write(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, &val);
	}

	return ret;
}

int rts_io_i2c_clr_bits(int i2c_nr, int slave_addr, int addr_type, int reg, int reg_len, uint8_t bit_mask)
{
	uint8_t val;
	int ret;

	ret = rts_io_i2c_bulk_read(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, &val);
	if (ret < 0)
		return ret;

	if (val & bit_mask) {
		val &= ~bit_mask;
		ret = rts_io_i2c_bulk_write(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, &val);
	}

	return ret;
}

int rts_io_i2c_update_bits(int i2c_nr, int slave_addr, int addr_type, int reg, int reg_len, uint8_t val, uint8_t bit_mask)
{
	uint8_t reg_val;
	int ret;

	ret = rts_io_i2c_bulk_read(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, &reg_val);
	if (ret < 0)
		return ret;

	val &= bit_mask;
	if ((reg_val & bit_mask) != val) {
		val = (reg_val & ~bit_mask) | val;
		ret = rts_io_i2c_bulk_write(i2c_nr, slave_addr, addr_type, reg, reg_len, 1, &val);
	}

	return ret;
}
