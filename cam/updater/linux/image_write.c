#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <mtd/mtd-user.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "image_write.h"

#ifndef __IPC_MODE_DEBUG__

static int _write_image_to_parttion(struct update_pack_image_desc* part, int image_fd)
{
    int fd                 = -1;
    int i                  = 0;
    int ret                = -1;
    int write_len          = 0;
    char device[256]       = { 0 };
    int cycle              = 0;
    unsigned long last_len = 0;
    unsigned char* buf     = NULL;
    int erase_offset       = 0;

    snprintf(device, 256, "/dev/mtd%d", part->mtd_number);

    printf("write image to %s\n", device);

    struct mtd_info_user info;
    struct erase_info_user einfo;

    if ((fd = open(device, O_RDWR)) < 0) {
        fprintf(stderr, "open %s failed\n", device);
        ret = -1;
        goto out;
    }

    if (ioctl(fd, MEMGETINFO, &info)) {
        fprintf(stderr, "get mtd infos failed\n");
        ret = -2;
        goto err;
    }

    /* warnning */
    printf("start to erase %s!\n", device);
    cycle    = info.size / info.erasesize;
    last_len = info.size % info.erasesize;

    for (i = 0; i < cycle; i++) {
        einfo.start  = erase_offset;
        einfo.length = info.erasesize;
        erase_offset += info.erasesize;

        if (ioctl(fd, MEMERASE, &einfo)) {
            fprintf(stderr, "erase mtd failed\n");
            ret = -3;
            goto err;
        }
        if (i % 5 == 0) {
            putchar('.');
            fflush(stdout);
        }

        if (i % 50 == 0) {
            printf("\n");
        }
    }

    if (last_len > 0) {
        einfo.start  = erase_offset;
        einfo.length = last_len;

        if (ioctl(fd, MEMERASE, &einfo)) {
            fprintf(stderr, "erase mtd failed\n");
            ret = -4;
            goto err;
        }

        putchar('.');
        fflush(stdout);
    }

    printf("\n");
    printf("erase done\n");

    buf = malloc(info.writesize);
    if (!buf) {
        fprintf(stderr, "malloc memory failed\n");
        ret = -5;
        goto err;
    }

    i = 0;

    do {

        ret = update_pack_read_image_fragment(part, buf, info.writesize, image_fd);
        if (ret < 0) {
            ret = -5;
            break;
        } else if (ret == 0) {
            ret = 0;
            break;
        }

        write_len += ret;

        if ((ret = write(fd, buf, ret)) < 0) {
            fprintf(stderr, "write %s failed\n", device);
            ret = -6;
            break;
        }

        if (i % 1000 == 0) {
            printf("\b\b\b\b%03d%%", ((write_len * 100) / part->image_len));
            fflush(stdout);
        }
        i++;
    } while (1);

    free(buf);
err:
    close(fd);
out:
    return ret;
}

static int _write_image_to_parttion_with_compare(struct update_pack_image_desc* part, int image_fd)
{
    int dev_fd              = -1;
    int i                   = 0;
    int ret                 = -1;
    int write_len           = 0;
    int diff_blocks         = 0;
    char device[256]        = { 0 };
    unsigned char* src_buf  = NULL;
    unsigned char* dest_buf = NULL;

    snprintf(device, 256, "/dev/mtd%d", part->mtd_number);

    printf("write image to %s\n", device);

    struct mtd_info_user info;
    struct erase_info_user erase;

    if ((dev_fd = open(device, O_RDWR | O_SYNC)) < 0) {
        fprintf(stderr, "open %s failed\n", device);
        ret = -1;
        goto out;
    }

    if (ioctl(dev_fd, MEMGETINFO, &info)) {
        fprintf(stderr, "get mtd infos failed\n");
        ret = -2;
        goto err;
    }

    src_buf = malloc(info.erasesize);
    if (!src_buf) {
        fprintf(stderr, "malloc memory failed\n");
        ret = -5;
        goto err;
    }

    dest_buf = malloc(info.erasesize);
    if (!dest_buf) {
        fprintf(stderr, "malloc memory failed\n");
        ret = -5;
        goto err1;
    }

    erase.start                     = 0;
    erase.length                    = info.erasesize;
    unsigned long current_dev_block = 0;

    i = 0;

    do {

        ret = update_pack_read_image_fragment(part, src_buf, info.erasesize, image_fd);
        if (ret < 0) {
            ret = -5;
            break;
        } else if (ret == 0) {
            ret = 0;
            break;
        }

        current_dev_block = lseek(dev_fd, 0, SEEK_CUR);

        if (read(dev_fd, dest_buf, ret) != ret) {
            ret = -6;
            break;
        }

        write_len += ret;

        if (memcmp(src_buf, dest_buf, ret)) {
            diff_blocks++;

            lseek(dev_fd, current_dev_block, SEEK_SET);
            if (ioctl(dev_fd, MEMERASE, &erase) < 0) {
                fprintf(stderr, "erase %s failed\n", device);
                ret = -6;
                break;
            }

            lseek(dev_fd, current_dev_block, SEEK_SET);
            if ((ret = write(dev_fd, src_buf, ret)) < 0) {
                fprintf(stderr, "write %s failed\n", device);
                ret = -6;
                break;
            }
        }

        erase.start += ret;

        if (i % 10 == 0) {
            printf("\b\b\b\b%03d%%", ((write_len * 100) / part->image_len));
            fflush(stdout);
        }
        i++;
    } while (1);

    printf("different blocks %d\n", diff_blocks);

    free(dest_buf);
err1:
    free(src_buf);
err:
    close(dev_fd);
out:
    return ret;
}
#endif

int partition_wirte_image(struct update_pack_image_desc* part, int fd)
{
#ifdef __IPC_MODE_DEBUG__

    char test_out[256] = { 0 };

    snprintf(test_out, 255, "testout/%s", part->partition_name);

    FILE*         fp = fopen(test_out, "wb+");
    unsigned char buffer[1024];
    if (fp) {
        do {
            memset(buffer, 0, 1024);
            int ret = update_pack_read_image_fragment(part, buffer, 1024, fd);
            if (ret <= 0) {
                break;
            }

            fwrite(buffer, ret, 1, fp);
        } while (1);
        fclose(fp);
    }
    return 0;
#else
    if (part->erase_all) {
        return _write_image_to_parttion(part, fd);
    } else {
        return _write_image_to_parttion_with_compare(part, fd);
    }
#endif
}