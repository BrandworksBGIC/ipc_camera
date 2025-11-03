#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "rts_io_gpio.h"
#include "rts_io_errno.h"

/* #define LOG_DEBG */
#ifdef LOG_DEBG
#define DEBG(...) printf(__VA_ARGS__)
#else
#define DEBG(...)
#endif

#define HWVER_PATH	"/sys/devices/platform/soc/rts_xb2/hwver"

enum {
	HWVER_RTS3915 = 1,
};

#define MAP_SIZE			(4*1024)

/* rts39x3 */
#define GPIO_BASE			0x18800000
#define GPIO_PULLCTRL			0x04
#define GPIO_SHARE_PULLCTRL		0x54
#define GPIO_SHARE_PULLCTRL1		0x70
#define GPIO_VIDEO_PULLCTRL		0x84

#define GPIO_SHARE_PULLCTRL_OFFSET	8
#define GPIO_SHARE_PULLCTRL1_OFFSET	23
#define GPIO_VIDEO_PULLCTRL_OFFSET	33

/* rts3915 */
#define RTS3915_GPIO_PULLCTRL		0x0008
#define RTS3915_GPIO_PULLCTRL_OFFSET	0x20

enum {
	GPIO_TYPE_GENERIC = 0,
	GPIO_TYPE_UART0,
	GPIO_TYPE_UART1,
	GPIO_TYPE_UART2,
	GPIO_TYPE_PWM,
	GPIO_TYPE_I2C,
	GPIO_TYPE_SDIO0,
	GPIO_TYPE_SDIO1,
	GPIO_TYPE_SSOR,
	GPIO_TYPE_DMIC,
	GPIO_TYPE_ADDA,
	GPIO_TYPE_I2S,
	GPIO_TYPE_SARADC,
	GPIO_TYPE_MIPITX,
	GPIO_TYPE_LVDS,
	GPIO_TYPE_USBH,
	GPIO_TYPE_USBD,
	GPIO_TYPE_USB3,
	GPIO_TYPE_NUMS,
};

struct sharepin_cfg_addr {
	int pinl;
	int pinh;
	int pint;
};

static struct sharepin_cfg_addr pincfgaddr[] = {
	{.pinl = 0, .pinh = 15, .pint = GPIO_TYPE_GENERIC},
	{.pinl = 16, .pinh = 19, .pint = GPIO_TYPE_UART0},
	{.pinl = 20, .pinh = 21, .pint = GPIO_TYPE_UART1},
	{.pinl = 22, .pinh = 23, .pint = GPIO_TYPE_UART2},
	{.pinl = 24, .pinh = 27, .pint = GPIO_TYPE_PWM},
	{.pinl = 28, .pinh = 29, .pint = GPIO_TYPE_I2C},
	{.pinl = 30, .pinh = 37, .pint = GPIO_TYPE_SDIO0},
	{.pinl = 38, .pinh = 45, .pint = GPIO_TYPE_SDIO1},
	{.pinl = 46, .pinh = 58, .pint = GPIO_TYPE_SSOR},
	{.pinl = 59, .pinh = 62, .pint = GPIO_TYPE_DMIC},
	{.pinl = 63, .pinh = 66, .pint = GPIO_TYPE_ADDA},
	{.pinl = 67, .pinh = 71, .pint = GPIO_TYPE_I2S},
	{.pinl = 72, .pinh = 75, .pint = GPIO_TYPE_SARADC},
	{.pinl = 76, .pinh = 85, .pint = GPIO_TYPE_MIPITX},
	{.pinl = 86, .pinh = 95, .pint = GPIO_TYPE_LVDS},
	{.pinl = 96, .pinh = 97, .pint = GPIO_TYPE_USBH},
	{.pinl = 98, .pinh = 99, .pint = GPIO_TYPE_USBD},
	{.pinl = 100, .pinh = 102, .pint = GPIO_TYPE_USB3},
};

static int get_hw_version(void)
{
	int fd, ret = 0;
	char buf[20];
	char *tmp;

	fd = open(HWVER_PATH, O_RDONLY);
	if (fd < 0) {
		printf("Open hwver path fail!\n");
		return -1;
	}

	ret = read(fd, buf, 20);
	if (ret == -1) {
		printf("read hwver failed, errno = %d\n", errno);
		return -1;
	}

	DEBG("hwver: %s\n", buf);

	tmp = strstr(buf, "rts3915");
	if (tmp)
		return HWVER_RTS3915;

	return 0;
}

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

static int system_gpio_write(char *dev, char *data, int nbytes)
{
	int fd;
	int ret;

	fd = open(dev, O_WRONLY);
	if (fd < 0) {
		printf("Can't Open '%s'\n", dev);
		return -ERR_IO_GPIO_OPENDEV_FAIL;
	}

	if (fcntl(fd, F_SETLKW, file_lock(F_WRLCK, SEEK_SET)) == -1) {
		close(fd);
		return -1;
	}
	ret = write(fd, data, nbytes);
	if (ret < 0) {
		printf("system_gpio_write failed errno = %d\n", errno);
		ret = -ERR_IO_GPIO_WRITE_FAIL;
	}

	if (fcntl(fd, F_SETLKW, file_lock(F_UNLCK, SEEK_SET)) == -1)
		ret = -1;

	close(fd);

	return ret;
}

static int system_gpio_read(char *dev, char *data, int nbytes)
{
	int fd;
	int ret;

	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		printf("Can't Open '%s'\n", dev);
		return -ERR_IO_GPIO_OPENDEV_FAIL;
	}

	if (fcntl(fd, F_SETLKW, file_lock(F_RDLCK, SEEK_SET)) == -1) {
		close(fd);
		return -1;
	}

	ret = read(fd, data, nbytes);
	if (ret < 0) {
		printf("system_gpio_read failed errno = %d\n", errno);
		ret = -ERR_IO_GPIO_READ_FAIL;
	}

	if (fcntl(fd, F_SETLKW, file_lock(F_UNLCK, SEEK_SET)) == -1)
		ret = -1;

	close(fd);

	return ret;
}

static int system_gpio_requested(int domain, int gpio)
{
	int ret, fd, req = 0;
	char data[5];
	char *dev = "/sys/class/gpio/export";
	char *free_dev = "/sys/class/gpio/unexport";

	snprintf(data, sizeof(data), "%d", gpio);
	fd = open(dev, O_WRONLY);
	if (fd < 0) {
		printf("Can't Open '%s'\n", dev);
		return -ERR_IO_GPIO_OPENDEV_FAIL;
	}

	if (fcntl(fd, F_SETLKW, file_lock(F_WRLCK, SEEK_SET)) == -1) {
		close(fd);
		return -1;
	}

	ret = write(fd, data, strlen(data));
	if (ret < 0) {
		ret = 0;
		req = 1;
	}

	if (fcntl(fd, F_SETLKW, file_lock(F_UNLCK, SEEK_SET)) == -1)
		ret = -1;

	close(fd);
	if (ret < 0)
		return ret;

	if (req)
		return GPIO_REQUESTED;

	ret =  system_gpio_write(free_dev, data, strlen(data));
	if (ret < 0)
		return ret;

	return GPIO_NOT_REQUESTED;
}

int rts_io_gpio_requested(int domain, int gpio)
{
	if (domain == SYSTEM_GPIO)
		return system_gpio_requested(domain, gpio);

	return -ERR_IO_GPIO_INVAL;
}

static struct rts_gpio *system_gpio_request(int domain, int gpio)
{
	int ret;
	char data[5];
	struct rts_gpio *rts_gpio;
	char *dev = "/sys/class/gpio/export";

	snprintf(data, sizeof(data), "%d", gpio);
	ret = system_gpio_write(dev, data, strlen(data));
	if (ret < 0)
		return NULL;

	rts_gpio = calloc(1, sizeof(*rts_gpio));
	if (!rts_gpio)
		return NULL;

	rts_gpio->domain = domain;
	rts_gpio->gpio = gpio;

	return rts_gpio;
}

struct rts_gpio *rts_io_gpio_request(int domain, int gpio)
{
	if (domain == SYSTEM_GPIO)
		return system_gpio_request(domain, gpio);

	return NULL;
}

static int system_gpio_free(struct rts_gpio *rts_gpio)
{
	int ret;
	char data[5];
	char *dev = "/sys/class/gpio/unexport";

	if (!rts_gpio)
		return 0;

	snprintf(data, sizeof(data), "%d", rts_gpio->gpio);

	free(rts_gpio);

	ret =  system_gpio_write(dev, data, strlen(data));
	if (ret < 0)
		return ret;

	return 0;
}

int rts_io_gpio_free(struct rts_gpio *rts_gpio)
{
	if (!rts_gpio)
		return -ERR_IO_GPIO_INVAL;

	if (rts_gpio->domain == SYSTEM_GPIO)
		return system_gpio_free(rts_gpio);

	return -ERR_IO_GPIO_INVAL;
}

static int system_gpio_set_value(struct rts_gpio *rts_gpio, int val)
{
	int ret;
	char dev[40];
	char data[4];

	snprintf(data, sizeof(data), "%d", val);
	snprintf(dev, sizeof(dev),
		"/sys/class/gpio/gpio%d/value", rts_gpio->gpio);

	ret = system_gpio_write(dev, data, 1);
	if (ret < 0)
		return ret;

	return 0;
}

int rts_io_gpio_set_value(struct rts_gpio *rts_gpio, int val)
{
	if (!rts_gpio)
		return -ERR_IO_GPIO_INVAL;

	if (rts_gpio->domain == SYSTEM_GPIO)
		return system_gpio_set_value(rts_gpio, val);

	return -ERR_IO_GPIO_INVAL;
}

static int system_gpio_get_value(struct rts_gpio *rts_gpio)
{
	int ret;
	char data[2] = {0};
	char dev[40];

	snprintf(dev, sizeof(dev),
		"/sys/class/gpio/gpio%d/value", rts_gpio->gpio);

	ret = system_gpio_read(dev, data, 1);
	if (ret < 0)
		return ret;

	return strtoul(data, NULL, 10);
}

int rts_io_gpio_get_value(struct rts_gpio *rts_gpio)
{
	if (!rts_gpio)
		return -ERR_IO_GPIO_INVAL;

	if (rts_gpio->domain == SYSTEM_GPIO)
		return system_gpio_get_value(rts_gpio);

	return -ERR_IO_GPIO_INVAL;
}

static int system_gpio_set_direction(struct rts_gpio *rts_gpio, int dir)
{
	char dev[40];
	char *data;
	int ret;

	snprintf(dev, sizeof(dev),
		"/sys/class/gpio/gpio%d/direction", rts_gpio->gpio);
	switch (dir) {
	case GPIO_INPUT:
		data = "in";
		break;
	case GPIO_OUTPUT:
		data = "out";
		break;
	default:
		return -ERR_IO_GPIO_INVAL;
	}

	ret = system_gpio_write(dev, data, strlen(data));
	if (ret < 0)
		return ret;

	return 0;
}

int rts_io_gpio_set_direction(struct rts_gpio *rts_gpio, int dir)
{
	if (!rts_gpio)
		return -ERR_IO_GPIO_INVAL;

	if (rts_gpio->domain == SYSTEM_GPIO)
		return system_gpio_set_direction(rts_gpio, dir);

	return -ERR_IO_GPIO_INVAL;
}

static int system_gpio_get_direction(struct rts_gpio *rts_gpio)
{
	int ret;
	char dev[40];
	char data[4];

	snprintf(dev, sizeof(dev),
		"/sys/class/gpio/gpio%d/direction", rts_gpio->gpio);

	ret = system_gpio_read(dev, data, 3);
	if (ret < 0)
		return ret;

	switch (data[0]) {
	case 'i':
		ret = GPIO_INPUT;
		break;
	case 'o':
		ret = GPIO_OUTPUT;
		break;
	default:
		ret = -ERR_IO_GPIO_INVAL;
		break;
	}

	return ret;
}

int rts_io_gpio_get_direction(struct rts_gpio *rts_gpio)
{
	if (!rts_gpio)
		return -ERR_IO_GPIO_INVAL;

	if (rts_gpio->domain == SYSTEM_GPIO)
		return system_gpio_get_direction(rts_gpio);

	return -ERR_IO_GPIO_INVAL;
}

static unsigned int *mmap_gpio(void)
{
	int fd;
	unsigned int *gpiobase = NULL;

	fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (fd == -1) {
		printf("Can't Open /dev/mem: %s\n", strerror(errno));
		return NULL;
	}

	gpiobase = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, GPIO_BASE);
	if (gpiobase == MAP_FAILED) {
		printf("mmap gpio failed: %s\n", strerror(errno));
		gpiobase = NULL;
	}

	close(fd);
	return gpiobase;
}

static inline void munmap_gpio(unsigned int *gpiobase)
{
	munmap(gpiobase, MAP_SIZE);
}

static void rts39x3_gpio_pullctrl_map(struct rts_gpio *rts_gpio,
			unsigned int *gpiobase, unsigned int **pullctrl,
			unsigned int *offset)
{
	if (rts_gpio->gpio < GPIO_SHARE_PULLCTRL_OFFSET) {
		*pullctrl = gpiobase + GPIO_PULLCTRL / 4;
		*offset = rts_gpio->gpio;
	} else if (rts_gpio->gpio < GPIO_SHARE_PULLCTRL1_OFFSET) {
		*pullctrl =  gpiobase + GPIO_SHARE_PULLCTRL / 4;
		*offset = rts_gpio->gpio - GPIO_SHARE_PULLCTRL_OFFSET;
	} else if (rts_gpio->gpio < GPIO_VIDEO_PULLCTRL_OFFSET) {
		*pullctrl = gpiobase + GPIO_SHARE_PULLCTRL1 / 4;
		*offset = rts_gpio->gpio - GPIO_SHARE_PULLCTRL1_OFFSET;
	} else {
		*pullctrl = gpiobase + GPIO_VIDEO_PULLCTRL / 4;
		*offset = rts_gpio->gpio - GPIO_VIDEO_PULLCTRL_OFFSET;
	}
}

static void rts3915_gpio_pullctrl_map(struct rts_gpio *rts_gpio,
			unsigned int *gpiobase, unsigned int **pullctrl,
			unsigned int *offset)
{
	int i;

	for (i = 0; i < GPIO_TYPE_NUMS; i++) {
		if (rts_gpio->gpio >= pincfgaddr[i].pinl &&
			rts_gpio->gpio <= pincfgaddr[i].pinh) {
			*pullctrl = gpiobase + (RTS3915_GPIO_PULLCTRL +
				pincfgaddr[i].pint *
				RTS3915_GPIO_PULLCTRL_OFFSET) / 4;
			*offset = rts_gpio->gpio - pincfgaddr[i].pinl;
		}
	}
}

static void get_gpio_pullctrl_map(struct rts_gpio *rts_gpio,
			unsigned int *gpiobase, unsigned int **pullctrl,
			unsigned int *offset)
{
	int hwver = 0;

	/* get hw version */
	hwver = get_hw_version();
	if (hwver < 0) {
		printf("unknown hwver!\n");
		return;
	}

	if (hwver == HWVER_RTS3915)
		rts3915_gpio_pullctrl_map(rts_gpio, gpiobase,
					pullctrl, offset);
	else
		rts39x3_gpio_pullctrl_map(rts_gpio, gpiobase,
					pullctrl, offset);
}

static int system_gpio_set_pull(struct rts_gpio *rts_gpio, int val)
{
	unsigned int *gpiobase = NULL, *pullctrl = NULL;
	unsigned int offset = 0;

	if (val < 0 || val > 2) {
		printf("invaled value %d\n", val);
		return -ERR_IO_GPIO_INVAL;
	}

	gpiobase = mmap_gpio();
	if (!gpiobase)
		return -ERR_IO_GPIO_MAP_FAIL;

	get_gpio_pullctrl_map(rts_gpio, gpiobase, &pullctrl, &offset);

	*pullctrl = (*pullctrl & (~(3 << offset * 2))) | (val << offset * 2);
	DEBG("pullctrl = 0x%08x, offset = %d, value = 0x%08x\n",
				(unsigned int)pullctrl, offset, *pullctrl);

	munmap_gpio(gpiobase);
	return 0;
}

int rts_io_gpio_set_pull(struct rts_gpio *rts_gpio, int val)
{
	if (!rts_gpio)
		return -ERR_IO_GPIO_INVAL;

	if (rts_gpio->domain == SYSTEM_GPIO)
		return system_gpio_set_pull(rts_gpio, val);

	return -ERR_IO_GPIO_INVAL;
}

static int system_gpio_get_pull(struct rts_gpio *rts_gpio)
{
	int val;
	unsigned int *gpiobase = NULL, *pullctrl = NULL;
	unsigned int offset = 0;

	gpiobase = mmap_gpio();
	if (!gpiobase)
		return -ERR_IO_GPIO_MAP_FAIL;

	get_gpio_pullctrl_map(rts_gpio, gpiobase, &pullctrl, &offset);

	val = (*pullctrl >> offset * 2) & 3;
	DEBG("pullctrl = 0x%08x, offset = %d, value = 0x%08x, val = %d\n",
			(unsigned int)pullctrl, offset, *pullctrl, val);

	munmap_gpio(gpiobase);
	return val;
}

int rts_io_gpio_get_pull(struct rts_gpio *rts_gpio)
{
	if (!rts_gpio)
		return -ERR_IO_GPIO_INVAL;

	if (rts_gpio->domain == SYSTEM_GPIO)
		return system_gpio_get_pull(rts_gpio);

	return -ERR_IO_GPIO_INVAL;
}
