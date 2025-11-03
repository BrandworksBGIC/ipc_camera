#ifndef __UPDATE_PACK_DECODE_H__
#define __UPDATE_PACK_DECODE_H__

#include "ipc_port_include.h"

#define IPC_FIRMWARE_FLAG "ippa"

enum {
    IPC_FIRMWARE_TYPE_NULL,
    IPC_FIRMWARE_TYPE_IMAGE_DATA,       //image data 
    IPC_FIRMWARE_TYPE_IMAGE_FILE_DESC,  //image file desc data
    IPC_FIRMWARE_TYPE_IMAGE_LIST_DESC,  // image list desc
    IPC_FIRMWARE_TYPE_FIRMWARE_VERSION, // fw version
};

struct update_pack_image_desc {
    char partition_name[8];
    unsigned char mtd_number;
    unsigned int partition_start_addr_at_flash;
    unsigned int partition_size;
    unsigned int image_start_addr_at_pack;
    unsigned int image_len;
    unsigned int cur_image_start_addr_offset;
    unsigned char erase_all;
};

ssize_t ipc_read(int fildes, void* buf, size_t nbyte);

ssize_t ipc_write(int fildes, void* buf, size_t nbyte);

off_t ipc_lseek(int fildes, off_t offset, int whence);

int ipc_close(int fildes);

struct update_pack_image_desc* update_pack_decode(char* pack_name, int* image_num, int* fd);

int update_pack_read_image_fragment(struct update_pack_image_desc* part, unsigned char* data, int data_len, int fd);


#endif