#include "ipc_24c02.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>


#define LOSCFG_HOST_TYPE_HIBVT
static u32 i2c_addr = 0x50;

static s32 _i2c_init(pv8 dev_node)
{
    clog_init("24c02", "");

    s32 fd = open(dev_node, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        ipcerror("Open %s error! errmsg=[%s]", dev_node, strerror(errno));
        return IPC_OPEN_ERROR;
    }

    s32 ret = ioctl(fd, I2C_SLAVE_FORCE, (i2c_addr));
    if (ret < 0) {
        ipcerror("I2C_SLAVE_FORCE error! errmsg=[%s]", strerror(errno));
        close(fd);
        return IPC_IOCTL_ERROR;
    }

    return fd;
}

static void _i2c_uninit(s32 fd)
{
    if (fd < 0) return ;
    close(fd);
}

static int _i2c_read_data(s32 fd , u32 addr, pu8 data, s32 data_len, s32 u32RegWidth)
{
    if (fd < 0) {
        return -1;
    }

    int s32Ret = 0;

    unsigned char aRecvbuf[4];

#ifdef LOSCFG_HOST_TYPE_HIBVT
    unsigned int u32SnsI2cAddr = (i2c_addr);
    struct i2c_rdwr_ioctl_data stRdwr;
    struct i2c_msg astMsg[2];
    memset(&stRdwr, 0x0, sizeof(stRdwr));
    memset(astMsg, 0x0, sizeof(astMsg));
#endif

    memset(aRecvbuf, 0x0, sizeof(aRecvbuf));

#ifdef LOSCFG_HOST_TYPE_HIBVT
    astMsg[0].addr  = u32SnsI2cAddr;
    astMsg[0].flags = 0;
    astMsg[0].len   = u32RegWidth;
    astMsg[0].buf   = aRecvbuf;

    astMsg[1].addr  = u32SnsI2cAddr;
    astMsg[1].flags = 0;
    astMsg[1].flags |= I2C_M_RD;
    astMsg[1].len = data_len;
    astMsg[1].buf = data;
    stRdwr.msgs   = &astMsg[0];
    stRdwr.nmsgs  = 2;
#endif

#ifdef LOSCFG_HOST_TYPE_HIBVT
    if (u32RegWidth == 2) {
        aRecvbuf[0] = (addr >> 8) & 0xff;
        aRecvbuf[1] = addr & 0xff;
    } else {
        aRecvbuf[0] = addr & 0xff;
    }

    s32Ret = ioctl(fd, I2C_RDWR, &stRdwr);
#else
    if (u32RegWidth == 2) {
        aRecvbuf[0] = addr & 0xff;
        aRecvbuf[1] = (addr >> 8) & 0xff;
    } else {
        aRecvbuf[0] = addr & 0xff;
    }
    s32Ret = read(g_fd, aRecvbuf, u32RegWidth + data_len);
#endif
    if (s32Ret < 0) {
        return -1;
    }

    return 0;
}

s32 ipc_24c02_read(pv8 dev_node, pu8 buf, s32 len, u32 reg_width)
{
    struct stat sb;

    // coverity[UNUSED_VALUE :SUPPRESS]
    int ret = IPC_FAILED;

    // coverity[TOCTOU :SUPPRESS]
    if (lstat(dev_node, &sb) == -1) {
        ipcerror("lstat");
        return IPC_OPEN_ERROR;
    }

    if ((sb.st_mode & S_IFMT) == S_IFREG) {
        // coverity[TOCTOU :SUPPRESS]
        FILE* fp = fopen(dev_node, "rb");
        if (!fp) {
            return IPC_OPEN_ERROR;
        }

        // coverity[VALUE_OVERWRITE :SUPPRESS]
        ret = fread(buf, 1, len, fp);

        fclose(fp);

        if (ret != len) {
            return IPC_READ_ERROR;
        }

        ret = IPC_SUCCESS;
    } else {
        s32 fd = _i2c_init(dev_node);
        if (fd < 0)
            return fd;

        // coverity[VALUE_OVERWRITE :SUPPRESS]
        ret = _i2c_read_data(fd, 0, buf, len, reg_width);

        _i2c_uninit(fd);
    }

    iphtrace(buf, len);

    return ret;
}
