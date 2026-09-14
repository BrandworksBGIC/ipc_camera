#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rts_io_adc.h"
#include "rts_io_errno.h"

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

int __io_adc_find_dir(const char* basepath, const char* partname, char* fullname, size_t fullname_size, int depth)
{
    DIR* dir;
    struct dirent* dp;
    int ret = -ERR_IO_ADC_NOT_MATCH;

    assert(basepath);
    assert(partname);
    assert(fullname);

    dir = opendir(basepath);
    if (!dir) {
        printf("Open dir %s fail\n", basepath);
        return -ERR_IO_ADC_OPENDEV_FAIL;
    }
    depth--;

    while ((dp = readdir(dir)) != NULL) {
        if ((dp->d_type != DT_DIR) || (!strcasecmp(dp->d_name, ".")) || (!strcasecmp(dp->d_name, "..")))
            continue;

        if (strstr(dp->d_name, partname)) {
            int len = snprintf(fullname, fullname_size, "%s/%s", basepath, dp->d_name);
            if (len < 0 || (size_t)len >= fullname_size) {
                ret = -ERR_IO_ADC_NOT_MATCH;
                continue;
            }
            ret = 0;
            break;
        }

        if (depth > 0) {
            char subpath[PATH_MAX];
            int len = snprintf(subpath, sizeof(subpath), "%s/%s", basepath, dp->d_name);
            if (len < 0 || (size_t)len >= sizeof(subpath)) {
                ret = -ERR_IO_ADC_NOT_MATCH;
                continue;
            }
            ret = __io_adc_find_dir(subpath, partname, fullname, fullname_size, depth);
            if (!ret)
                break;
        }
    }

    if (dir) {
        closedir(dir);
        // coverity[UNUSED_VALUE :SUPPRESS]
        dir = NULL;
    }

    return ret;
}

static int rts_io_adc_read(char* dev, char* data, int nbytes)
{
    int fd;
    int ret;

    fd = open(dev, O_RDONLY);
    if (fd < 0) {
        printf("Can't Open '%s'\n", dev);
        return -ERR_IO_ADC_OPENDEV_FAIL;
    }

    ret = read(fd, data, nbytes);
    if (ret < 0) {
        printf("Can't read %s\n", dev);
        ret = -ERR_IO_ADC_READ_FAIL;
    }
    close(fd);

    return ret;
}

int rts_io_adc_get_value(int adc_channel)
{
    int ret;
    char data[6];
    char dev[PATH_MAX];
    int remain;

    ret = __io_adc_find_dir("/sys/devices/platform/", "saradc", dev, sizeof(dev), 2);
    if (ret < 0) {
        printf("Can't find dir:\"saradc\"\n");
        return ret;
    }

    remain = sizeof(dev) - strlen(dev);
    ret    = snprintf(dev + strlen(dev), remain, "/in%d_input", adc_channel);
    if (ret < 0 || ret >= remain) {
        printf("Get dev name fail\n");
        return ret;
    }

    ret = rts_io_adc_read(dev, data, sizeof(data));
    if (ret < 0)
        return ret;

    return strtoul(data, NULL, 10);
}
