#include "ipc_utf.h"
#include "ipc_misc.h"

#define MAX_UNICODE                                                                                                    \
    0x10FFFFu // Maximum defined Unicode value for UCS4; further expansions would require changes to UTF-16.

/********* Improved from the utf8_decode function in lua lutf8lib.c *********/
s32 ipc_utf8_decode(pv8 utf8, pu32 unicode)
{
    if (!utf8 || !utf8[0] || !unicode)
        return IPC_INVALID_ARGS;

    pu8 src = (pu8)utf8;
    if (src[0] < 0x80) { // ASCII
        *unicode = src[0];
        return 1; // ASCII is 1 byte
    }

    cu32 limits[] = { ~(u32)0, 0x80, 0x800, 0x10000u, /* 0x200000u, 0x4000000u */ };
    u32 result    = 0;
    s32 count     = 0;
    u8 first      = src[0];

    while (first & 0x40) {             /* Only look at the 7th bit */
        u8 follow = src[++count];      /* Read the next byte */
        if ((follow & 0xC0) != 0x80) { /* Invalid byte sequence */
            return IPC_PARSE_FAILED;
        }
        result = (result << 6) | (follow & 0x3F); /* Each byte's lower 6 bits are valid information */
        first <<= 1;
    }

    result |= (first & 0x7F)
              << (count
                  * 5); /* Combine with the valid information from the first byte (since first has already been shifted
                           by count, a total shift of count * 6 is needed, so we only need to shift by count * 5) */
    if (count >= ARRSIZE(limits) || result < limits[count]
        || result > MAX_UNICODE /* UTF-8 can be up to 6 bytes long, excluding the first byte, the remaining count cannot
                                   exceed 5 */
        || (0xD800u <= result
            && result <= 0xDFFFu)) { /* Check for invalid code points; (Unicode surrogate: due to the previous design
                                        limitation of Unicode being a maximum of 65535, UTF-16 suffered, so Unicode
                                        reserved D800-DFFF for UTF-16 to fend for itself)*/
        return IPC_PARSE_FAILED;      /* Invalid byte sequence */
    }

    *unicode = result;
    return count + 1; // Including the first byte
}