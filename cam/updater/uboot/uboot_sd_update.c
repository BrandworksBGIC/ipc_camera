#include "ipc_uboot_port.h"

#include "../update_pack_decode.h"

#include "../ff_fat/source/ff.h"

#define DRIVER_DISK "0:"

static char* aufile[] = {
    IPC_ARCH_PLATFORM_NAME "all." IPC_FIRMWARE_FLAG,
    IPC_ARCH_PLATFORM_NAME "sd." IPC_FIRMWARE_FLAG,
    IPC_ARCH_PLATFORM_NAME "ota." IPC_FIRMWARE_FLAG,
};

#define __PRINT_MACRO(x) #x
#define PRINT_MACRO(x) #x "="__PRINT_MACRO(x)
#pragma message(PRINT_MACRO(IPC_ARCH_PLATFORM_NAME))

static off_t cur_update_package_offset = 0;
static ssize_t cur_update_package_len  = 0;
static void* cur_update_package_buf    = NULL;

#define ALIGN_LEN(value)                                                                                               \
    value = value + (CONFIG_SYS_CACHELINE_SIZE - ((unsigned int)value & (CONFIG_SYS_CACHELINE_SIZE - 1)));

static void _init_update_package(long size)
{
    cur_update_package_offset = 0;
    cur_update_package_len    = size;
}

static void ipc_wtd_start(unsigned int second)
{
}

static void ipc_wtd_stop(void)
{
}

ssize_t ipc_read(int fildes, void* buf, size_t nbyte)
{

    size_t left = cur_update_package_len - cur_update_package_offset;

    if (left == 0) {
        // printf("1:%d:%d\n", cur_update_package_offset, nbyte);
        return 0;
    }

    if (left < 0) {
        // printf("2:%d:%d\n", cur_update_package_offset, nbyte);
        return -1;
    }

    if (left < nbyte) {
        // printf("3:%d:%d\n", cur_update_package_offset, nbyte);
        nbyte = left;
    }

    unsigned char* src_buf = cur_update_package_buf + cur_update_package_offset;

    memcpy(buf, src_buf, nbyte);

    cur_update_package_offset += nbyte;

    return nbyte;
}

ssize_t ipc_write(int fildes, void* buf, size_t nbyte)
{

    unsigned char* dest_buf = cur_update_package_buf + cur_update_package_offset;

    memcpy(dest_buf, buf, nbyte);

    cur_update_package_offset += nbyte;

    return nbyte;
}

off_t ipc_lseek(int fildes, off_t offset, int whence)
{
    if (SEEK_CUR == whence) {
        cur_update_package_offset += offset;
    } else if (SEEK_SET == whence) {
        cur_update_package_offset = offset;
    }

    return cur_update_package_offset;
}

int ipc_close(int fildes)
{
    cur_update_package_offset = 0;
    cur_update_package_len    = 0;

    return 0;
}

static int partition_do_update(struct update_pack_image_desc* part, char* pbuf)
{
    /*
     * erase the address range.
     */
    // printf("flash erase...[%x:%x]\n", part->partition_start_addr_at_flash, part->partition_size);

    ipc_spi_flash_erase(part->partition_start_addr_at_flash, part->partition_size);

    ipc_spi_flash_write(part->partition_start_addr_at_flash, pbuf, part->image_len);

    return 0;
}

static int check_partition_is_diff(struct update_pack_image_desc* part, unsigned char* pbuf)
{

    int ret = 0;

    int align_left = part->image_len & (CONFIG_SYS_CACHELINE_SIZE - 1);

    int align_len = part->image_len + (CONFIG_SYS_CACHELINE_SIZE - align_left);

    ipc_spi_flash_read(part->partition_start_addr_at_flash, pbuf, align_len);

    ret = memcmp(pbuf, cur_update_package_buf + part->image_start_addr_at_pack, part->image_len);

    printf("partition %s diff:%d\n", part->partition_name, ret);

    ret = (ret != 0 ? 1 : 0);
    return ret;
}

static int ipc_fatfs_read(char* filename, unsigned char* readbuff, unsigned int buffsize)
{
    int ret = -1;
    FRESULT res_sd_user;
    UINT fnum;        
    FIL fnew = { 0 }; 

    char path[128] = { 0 };
    snprintf(path, sizeof(path), "%s%s", DRIVER_DISK, filename);
    // printf("fatfs read file path: %s\n", path);

    res_sd_user = f_open(&fnew, path, FA_READ);
    if (res_sd_user == FR_OK) {
        printf("open file %s success! start read data!\r\n", filename);
        res_sd_user = f_read(&fnew, readbuff, buffsize, &fnum);

        if (res_sd_user == FR_OK) {
            printf("data read ok, size: %d\r\n", fnum);
            // printf("data: %s\r\n", buffsize);
            ret = fnum;
        } else {
            printf("data read failed\r\n");
        }

        f_close(&fnew);
    } else {
        printf("Warning, file: %s does not exist\n", path);
    }

    return ret;
}


#define UPDATE_MALLOC_BUFF_SIZE (16 * 1024 * 1024)
static int do_sdcard_update(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
    int i                    = 0;
    int need_reboot          = 0;
    int device_num           = 1;
    unsigned int otabakpaddr = 0;
    char cmd[256]            = { 0 };
#define OTA_BAK_FLAG 0x616f7667

  struct __attribute__((packed)) {
        unsigned int flag;
        unsigned short desc_len;
        unsigned int startaddr;
        unsigned int update_pack_len;
    } ota_bak_head = { 0 };

    ipc_spi_flash_init();

    mdelay(300);

    char* commandline = ipc_env_get("bootargs");

    printf("otacommandline: %s\n", commandline);
    
    if (commandline && strstr(commandline, "mtdparts=") && strstr(commandline, "otabak")) {
        printf("is have otabak partition\n");
        char* s = strstr(commandline, "mtdparts=");
        s       = strstr(s, ":");
        s++;
        while (s) {
            ssize_t cur_part_size = ustrtoul(s, &s, 0);
            otabakpaddr += cur_part_size;

            // printf("%s:%llu\n", s, otabakpaddr);

            if (s[0] == 'k' || s[0] == 'K') {
                s++;
            }

            // printf("%s\n",s);

            if (strncmp(s, "(otabak)", 8) == 0) {
                otabakpaddr -= cur_part_size;
                break;
            }

            s = strstr(s, ",");
            if (!s) {
                break;
            }
            s++;
        }

        ipc_spi_flash_read(otabakpaddr, (void*)&ota_bak_head, sizeof(ota_bak_head));

        printf("ota startaddr:0x%x:0x%x:[0x%x]\n", ota_bak_head.startaddr, ota_bak_head.update_pack_len,
               ota_bak_head.flag);
    }

    printf("otabakpaddr:0x%x\n", otabakpaddr);

    if (otabakpaddr <= 0) {
        ota_bak_head.flag = 0x0;
    }

    if (ota_bak_head.flag == OTA_BAK_FLAG) {
        goto OTA_UPDATA;
    }

    debug("device name %s!\n", "mmc");

    FRESULT res_sd_user;
    FATFS FatFs_user; 
    UINT fnum;        
    FIL fnew = { 0 }; 

    res_sd_user = f_mount(&FatFs_user, DRIVER_DISK, 1);
    // printf("====== f_mount ret:%d ======\n", res_sd_user);

    if (FR_OK != res_sd_user) {
        printf("Error, mout fatfs failed, error num: %d\n", res_sd_user);
        return 0;
    }

OTA_UPDATA:

    cur_update_package_buf = memalign(CONFIG_SYS_CACHELINE_SIZE, UPDATE_MALLOC_BUFF_SIZE);
    if (cur_update_package_buf == NULL) {
        printf("malloc error\n");
        return 0;
    }

    printf("malloc for update package [%p]\n", cur_update_package_buf);

    for (i = 0; i < sizeof(aufile) / sizeof(aufile[0]); i++) {
        long sz                              = 0;
        int image_num                        = 0;
        int image_index                      = 0;
        int out_fd                           = 0;
        struct update_pack_image_desc* descs = NULL;

        if (ota_bak_head.flag == OTA_BAK_FLAG) {

            ipc_spi_flash_read(otabakpaddr + ota_bak_head.startaddr, cur_update_package_buf,
                              ota_bak_head.update_pack_len);

            sz = ota_bak_head.update_pack_len;

            printf("ota_read:%ld\n", sz);

        } else {
            ipc_wtd_start(10);

            sz = ipc_fatfs_read(aufile[i], cur_update_package_buf, UPDATE_MALLOC_BUFF_SIZE);

            ipc_wtd_stop();

            if (sz <= 0) {
                continue;
            }
            printf("file_fat_read:%ld\n", sz);
        }

        _init_update_package(sz);

        descs = update_pack_decode(NULL, &image_num, &out_fd);

        if (out_fd == -3) {
            if (ota_bak_head.flag == OTA_BAK_FLAG) {
                break;
            } else {
                continue;
            }
        }

        printf("descs:%p\n", descs);

        unsigned int max_image_len = 0;
        for (image_index = 0; image_index < image_num; image_index++) {
            max_image_len = descs[image_index].image_len > max_image_len ? descs[image_index].image_len : max_image_len;
        }

        ALIGN_LEN(max_image_len);

        printf("max_image_len:%u\n", max_image_len);

        void* image_buf = memalign(CONFIG_SYS_CACHELINE_SIZE, max_image_len);
        if (image_buf == NULL) {
            printf("malloc for image error\n");
            goto exit;
        }

        for (image_index = 0; image_index < image_num; image_index++) {
            struct update_pack_image_desc* desc = &descs[image_index];
            // printf("mtd_number:%d\n", desc[image_index].mtd_number);
            printf("partition_name:%s\n", desc->partition_name);
            // printf("image_start_addr_at_pack:%d\n", desc->image_start_addr_at_pack);
            // printf("image_len:%d\n", desc->image_len);
            // printf("partition_size:%d\n", desc->partition_size);
            // printf("partition_start_addr_at_flash:%d\n", desc->partition_start_addr_at_flash);

            if (check_partition_is_diff(desc, image_buf)) {
                memcpy(image_buf, cur_update_package_buf + desc->image_start_addr_at_pack, desc->image_len);

                if (desc->erase_all) {
                    partition_do_update(desc, image_buf);
                } else {
                    ipc_spi_flash_update(desc->partition_start_addr_at_flash, image_buf, desc->image_len);
                }

                if (strncmp(desc->partition_name, "uboot", 5) == 0) {
                    need_reboot = 1;
                }
            }
        }

        free(image_buf);

        break;
    }

    
    if (ota_bak_head.flag == OTA_BAK_FLAG) {
        ipc_spi_flash_erase(otabakpaddr, 0x1000);
    }

exit:

    if (need_reboot) {
        printf("updated uboot, reboot now!\n");
        run_command("reset", 0);
    }

    free(cur_update_package_buf);
    return 0;
}

U_BOOT_CMD(
	ipcsdupdate,	CONFIG_SYS_MAXARGS,	1,	do_sdcard_update,
	"just run",
	""
);
