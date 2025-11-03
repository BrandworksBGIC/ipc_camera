#include "update_pack_decode.h"

#include "ed25519/src/ed25519.h"
#include "ed25519/src/sha512.h"


static void hexdump(char* title, const void* p, int size)
{
    const uint8_t* c = p;
    printf("title:%s\n", title);

    printf("Dumping %d bytes from %p:\n", size, p);

    while (size > 0) {
        unsigned i;

        for (i = 0; i < 16; i++) {
            if (i < size)
                printf("%02x ", c[i]);
            else
                printf("   ");
        }

        for (i = 0; i < 16; i++) {
            if (i < size)
                printf("%c", c[i] >= 32 && c[i] < 127 ? c[i] : '.');
            else
                printf(" ");
        }

        printf("\n");

        c += 16;

        if (size <= 16)
            break;

        size -= 16;
    }
}

#ifdef IPC_IS_UBOOT_BUILD
static int _open_update_pack(char* pack_name)
{
    return 0;
}

#else

ssize_t ipc_read(int fildes, void* buf, size_t nbyte)
{
    return read(fildes, buf, nbyte);
}

off_t ipc_lseek(int fildes, off_t offset, int whence)
{
    return lseek(fildes, offset, whence);
}

ssize_t ipc_write(int fildes, void* buf, size_t nbyte)
{
    return write(fildes, buf, nbyte);
}

int ipc_close(int fildes)
{
    return close(fildes);
}

static int _open_update_pack(char* pack_name)
{
    int fd = open(pack_name, O_RDWR);
    if (fd < 0) {
        fd = open(pack_name, O_RDONLY);
    }
    return fd;
}

#endif


static int read_update_pack_file_head(int fd)
{
    int ret                    = 0;
    char firmware_flag[5] = { 0 };
    unsigned char sign[64]     = { 0 };

    ret = ipc_read(fd, firmware_flag, 4);
    if (ret != 4) {
        printf("read firmware flag error\n");
        return -1;
    }

    if (strncmp(IPC_FIRMWARE_FLAG, firmware_flag, 4) != 0) {
        printf("firmware flag error[%s]\n", firmware_flag);
        return -2;
    }

    ret = ipc_read(fd, sign, 64);
    if (ret != 64) {
        printf("read firmware sign error\n");
        return -1;
    }

    sha512_context md = { 0 };
    sha512_init(&md);

    do {
        unsigned char buffer[512] = { 0 };
        ret                       = ipc_read(fd, buffer, 512);
        if (ret < 0) {
            return -1;
        } else if (ret == 0) {
            break;
        }

        sha512_update(&md, buffer, ret);
    } while (1);

    unsigned char sha512sum[64] = { 0 };

    sha512_final(&md, sha512sum);

    unsigned char ed25519_pub[32] = {
        0xf5, 0x89, 0x06, 0xa5, 0xcf, 0xc4, 0x00, 0x13, 0x9f, 0x43, 0x13, 0xb7,
        0x84, 0xb0, 0x4f, 0x71, 0x48, 0x9b, 0x2e, 0xc6, 0x35, 0x5e, 0x09, 0xd2,
        0xd0, 0x38, 0x23, 0x27, 0xc1, 0x71, 0xda, 0xb9
    };

    hexdump("ed25519_pub", ed25519_pub, 32);
    
    if (!ed25519_verify(sign, sha512sum, 64, ed25519_pub)) {
        hexdump("sign", sign, 64);
        hexdump("sha512", sha512sum, 64);
        printf("sign error\n");
        return -3;
    }

    ipc_lseek(fd, 64 + 4, SEEK_SET);

    return 0;
}

static int read_pack_tlv_head(char* name, int fd, unsigned short* type, unsigned int* data_len)
{
    int ret = 0;
    ret     = ipc_read(fd, type, 2);
    // hexdump("tlv type", type, 2);
    if (ret != 2) {
        printf("[%s]read tlv type error:%d\n", name, ret);
        return -1;
    }
    ret = ipc_read(fd, data_len, 4);
    // hexdump("tlv data_len", data_len, 4);
    if (ret != 4) {
        printf("read tlv len error:%d\n", ret);
        return -1;
    }

    return 0;
}

static int _read_item(void* item_val, unsigned char* buffer, int offset, int max_size, int item_size)
{
    if (offset + item_size > max_size) {
        return offset;
    }
    memcpy(item_val, buffer + offset, item_size);

    return offset + item_size;
}

static int read_image_list(struct update_pack_image_desc* part_list, int image_num, int fd)
{
    int ret = 0;

    int i = 0;
    for (i = 0; i < image_num; i++) {
        unsigned short type   = 0;
        unsigned int data_len = 0;
        unsigned int offset   = 0;

        if (read_pack_tlv_head("image_desc", fd, &type, &data_len) < 0) {
            ret = -1;
            goto err_exit;
        }
        // printf("%s:%d:%d\n", __func__, type, data_len);
        unsigned char buffer[data_len];
        if (ipc_read(fd, buffer, data_len) != data_len) {
            ret = -2;
            goto err_exit;
        }

        offset = _read_item(&part_list[i].partition_name, buffer, offset, data_len, 8);
        offset = _read_item(&part_list[i].mtd_number, buffer, offset, data_len, 1);
        offset = _read_item(&part_list[i].partition_start_addr_at_flash, buffer, offset, data_len, 4);
        offset = _read_item(&part_list[i].partition_size, buffer, offset, data_len, 4);
        offset = _read_item(&part_list[i].image_start_addr_at_pack, buffer, offset, data_len, 4);
        offset = _read_item(&part_list[i].image_len, buffer, offset, data_len, 4);
        offset = _read_item(&part_list[i].erase_all, buffer, offset, data_len, 1);
    }

err_exit:
    return ret;
}

struct update_pack_image_desc* update_pack_decode(char* pack_name, int* image_num, int* out_fd)
{
    int ret = 0;
    int fd  = 0;

    struct update_pack_image_desc* part_list = NULL;

    fd = _open_update_pack(pack_name);
    if (fd < 0) {
        printf("open update pack [%s]error\n", pack_name);
        return NULL;
    }


    ipc_lseek(fd, 0, SEEK_SET);

    *out_fd = fd;

    ret = read_update_pack_file_head(fd);
    if (ret < 0) {
        *out_fd = ret;
        goto err_exit;
    }

    do {
        unsigned short type   = 0;
        unsigned int data_len = 0;

        if (read_pack_tlv_head("type", fd, &type, &data_len) < 0) {
            break;
        }

        if (type == IPC_FIRMWARE_TYPE_IMAGE_LIST_DESC) {
            ret = ipc_read(fd, image_num, 4);
            if (ret != 4) {
                goto err_exit;
            }

            printf("image num:%d\n", *image_num);

            part_list = calloc(*image_num, sizeof(struct update_pack_image_desc));
            if (part_list == NULL) {
                printf("%s", "calloc error\n");
                goto err_exit;
            }

            memset(part_list, 0, sizeof(struct update_pack_image_desc) * (*image_num));

            if (read_image_list(part_list, *image_num, fd) < 0) {
                goto err_exit;
            }

        } else {
            if (data_len != 0) {
                ipc_lseek(fd, data_len, SEEK_CUR);
            }
        }

    } while (1);

    return part_list;

err_exit:
    ipc_close(fd);
    return NULL;
}

int update_pack_read_image_fragment(struct update_pack_image_desc* part, unsigned char* data, int data_len, int fd)
{
    int offset = part->image_start_addr_at_pack + part->cur_image_start_addr_offset;
    int ret    = 0;
    int left   = 0;

    if (part->cur_image_start_addr_offset >= part->image_len) {
        return -2;
    }

    left = part->image_len - part->cur_image_start_addr_offset;

    data_len = data_len < left ? data_len : left;

    ipc_lseek(fd, offset, SEEK_SET);

    ret = ipc_read(fd, data, data_len);
    if (ret < 0) {
        printf("read image data error\n");
        return -1;
    }

    ret = ret < data_len ? ret : data_len;
    part->cur_image_start_addr_offset += ret;

    return ret;
}