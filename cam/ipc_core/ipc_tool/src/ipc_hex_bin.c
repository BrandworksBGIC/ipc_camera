#include "ipc_hex_bin.h"
#include "ipc_std.h"

static u8 HexCharToBinBinChar(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 0xff;
}

s32 ipc_hex_to_bin(pv8 src_string, pu8 dst_buffer, s32 buffer_size)
{
    s32 src_len = strlen(src_string);
    s32 i       = 0;
    s32 j       = 0;
    if (src_len % 2) {
        return IPC_INVALID_ARGS;
    }

    if (src_len / 2 > buffer_size) {
        return IPC_INVALID_ARGS;
    }

    for (i = 0; i < src_len; i += 2) {
        u8 tmp = 0;
        tmp    = (u8)((HexCharToBinBinChar(src_string[i + 0]) << 4) | HexCharToBinBinChar(src_string[i + 1]));
        dst_buffer[j] = tmp;
        j++;
    }

    return j;
}

s32 ipc_bin_to_hex(pu8 src_data, s32 data_len, pv8 dst_buffer, s32 buffer_size)
{
    if (buffer_size < data_len * 2) {
        return IPC_INVALID_ARGS;
    }
    pcv8 hextable = "0123456789abcdef";

    s32 i = 0;
    s32 j = 0;

    for (i = 0; i < data_len; i ++) {
        dst_buffer[j] = hextable[src_data[i] >> 4];
        j++;
        dst_buffer[j] = hextable[src_data[i] & 0x0f];
        j++;
    }

    return j;
}