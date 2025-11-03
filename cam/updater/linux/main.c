#include <fcntl.h>
#include <linux/reboot.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "image_write.h"
#include "update_pack_decode.h"


static void _update_mtd_parts(int fd, struct update_pack_image_desc* desc, int image_num)
{
    int i = 0;
    for (i = 0; i < image_num; i++) {
        printf("mtd_number:%d\n", desc[i].mtd_number);
        printf("partition_name:%s\n", desc[i].partition_name);
        printf("image_start_addr_at_pack:%d\n", desc[i].image_start_addr_at_pack);
        printf("image_len:%d\n", desc[i].image_len);
        printf("partition_size:%d\n", desc[i].partition_size);
        printf("partition_start_addr_at_flash:%d\n", desc[i].partition_start_addr_at_flash);

        partition_wirte_image(&desc[i], fd);
    }
}

int main(int argc, char** argv)
{
    int image_num                       = 0;
    int fd                              = 0;
    struct update_pack_image_desc* desc = NULL;

    desc = update_pack_decode(argv[1], &image_num, &fd);
    if (desc == NULL) {
        printf("update pack_desc fail\n");
        goto exit;
    }

    _update_mtd_parts(fd, desc, image_num);

    ipc_close(fd);

exit:

#ifndef __IPC_MODE_DEBUG__
    sync();

    reboot(LINUX_REBOOT_CMD_RESTART);
#endif
    return 0;
}
