#include "ipc_g711.h"

#define SIGN_BIT   0x80    /* Sign bit for a A-law byte. */
#define QUANT_MASK 0xf     /* Quantization field mask. */
#define NSEGS      8       /* Number of A-law segments. */
#define SEG_SHIFT  4       /* Left shift for segment number. */
#define SEG_MASK   0x70    /* Segment field mask. */

static s32 seg_aend[8] = {0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF};
static s32 seg_uend[8] = {0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF};

static s32 _search(s32 val, ps32 table, s32 size)
{
    s32 idx = 0;
    for (; idx < size; idx++) {
        if (val <= *table++) break;
    }
    return idx;
}

/*
 * linear2alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
 *
 * linear2alaw() accepts an 16-bit integer and encodes it as A-law data.
 *
 *  Linear Input Code Compressed Code
 * ------------------------ ---------------
 * 0000000wxyza   000wxyz
 * 0000001wxyza   001wxyz
 * 000001wxyzab   010wxyz
 * 00001wxyzabc   011wxyz
 * 0001wxyzabcd   100wxyz
 * 001wxyzabcde   101wxyz
 * 01wxyzabcdef   110wxyz
 * 1wxyzabcdefg   111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
static s32 linear2alaw(s32 pcm_val)        /* 2's complement (16-bit range) */
{
    s32 mask; 
    s32 seg;  
    s32 aval;

    pcm_val = pcm_val >> 3;

    if (pcm_val >= 0) {
        mask = 0xD5;  /* sign (7th) bit = 1 */
    } else {
        mask = 0x55;  /* sign bit = 0 */
        pcm_val = -pcm_val - 1;
    }

    /* Convert the scaled magnitude to segment number. */
    seg = _search(pcm_val, seg_aend, 8);

    /* Combine the sign, segment, and quantization bits. */

    if (seg >= 8)  /* out of range, return maximum value. */
        return (0x7F ^ mask);
    else {
        aval = seg << SEG_SHIFT;
        if (seg < 2)
            aval |= (pcm_val >> 1) & QUANT_MASK;
        else
            aval |= (pcm_val >> seg) & QUANT_MASK;
        return (aval ^ mask);
    }
}

/*
 * alaw2linear() - Convert an A-law value to 16-bit linear PCM
 *
 */
static s32 alaw2linear(s32 a_val)  
{
    s32 t;
    s32 seg;

    a_val ^= 0x55;
    t = (a_val & QUANT_MASK) << 4;
    seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
    switch (seg) {
        case 0:
            t += 8;
            break;
        case 1:
            t += 0x108;
            break;
        default:
            t += 0x108;
            t <<= seg - 1;
    }
    return ((a_val & SIGN_BIT) ? t : -t);
}

#define BIAS  0x84  /* Bias for linear code. */
#define CLIP  8159

/*
 * linear2ulaw() - Convert a linear PCM value to u-law
 *
 * In order to simplify the encoding process, the original linear magnitude
 * is biased by adding 33 which shifts the encoding range from (0 - 8158) to
 * (33 - 8191). The result can be seen in the following encoding table:
 *
 * Biased Linear Input Code Compressed Code
 * ------------------------ ---------------
 * 00000001wxyza   000wxyz
 * 0000001wxyzab   001wxyz
 * 000001wxyzabc   010wxyz
 * 00001wxyzabcd   011wxyz
 * 0001wxyzabcde   100wxyz
 * 001wxyzabcdef   101wxyz
 * 01wxyzabcdefg   110wxyz
 * 1wxyzabcdefgh   111wxyz
 *
 * Each biased linear code has a leading 1 which identifies the segment
 * number. The value of the segment number is equal to 7 minus the number
 * of leading 0's. The quantization interval is directly available as the
 * four bits wxyz.  * The trailing bits (a - h) are ignored.
 *
 * Ordinarily the complement of the resulting code word is used for
 * transmission, and so the code word is complemented before it is returned.
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
static s32 linear2ulaw(s32 pcm_val) /* 2's complement (16-bit range) */
{
    s32 mask;
    s32 seg;
    s32 uval;

    /* Get the sign and the magnitude of the value. */
    pcm_val = pcm_val >> 2;
    if (pcm_val < 0) {
        pcm_val = -pcm_val;
        mask = 0x7F;
    } else {
        mask = 0xFF;
    }

    if ( pcm_val > CLIP ) pcm_val = CLIP; /* clip the magnitude */
    pcm_val += (BIAS >> 2);

    /* Convert the scaled magnitude to segment number. */
    seg = _search(pcm_val, seg_uend, 8);

    /*
     * Combine the sign, segment, quantization bits;
     * and complement the code word.
     */
    if (seg >= 8)  /* out of range, return maximum value. */
        return (0x7F ^ mask);
    else {
        uval = (seg << 4) | ((pcm_val >> (seg + 1)) & 0xF);
        return (uval ^ mask);
    }
}

/*
 * ulaw2linear() - Convert a u-law value to 16-bit linear PCM
 *
 * First, a biased linear code is derived from the code word. An unbiased
 * output can then be obtained by subtracting 33 from the biased code.
 *
 * Note that this function expects to be passed the complement of the
 * original code word. This is in keeping with ISDN conventions.
 */
static s32 ulaw2linear(s32 u_val)
{
    s32 t;

    /* Complement to obtain normal u-law value. */
    u_val = ~u_val;

    /*
     * Extract and bias the quantization bits. Then
     * shift up by the segment number and subtract out the bias.
     */
    t = ((u_val & QUANT_MASK) << 3) + BIAS;
    t <<= (u_val & SEG_MASK) >> SEG_SHIFT;

    return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
}

s32 ipc_g711u_decode(vptr dest, vptr src, s32 src_len)
{
    for (s32 idx = 0; idx < src_len; idx++) {
        ((ps16)dest)[idx] = ulaw2linear(((pu8)src)[idx]); /* 1 src to 2 dest  */
    }
    return src_len * 2;
}

s32 ipc_g711u_encode(vptr dest, vptr src, s32 src_len)
{
    for (s32 idx = 0; idx < src_len / 2; idx++) {
        ((pu8)dest)[idx] = linear2ulaw(((ps16)src)[idx]); /* 2 src to 1 dest  */
    }
    return src_len / 2;
}


s32 ipc_g711a_decode(vptr dest, vptr src, s32 src_len)
{
    for (s32 idx = 0; idx < src_len; idx++) {
        ((ps16)dest)[idx] = alaw2linear(((pu8)src)[idx]); /* 1 src to 2 dest  */
    }
    return src_len * 2;
}

s32 ipc_g711a_encode(vptr dest, vptr src, s32 src_len)
{
    for (s32 idx = 0; idx < src_len / 2; idx++) {
        ((pu8)dest)[idx] = linear2alaw(((ps16)src)[idx]); /* 2 src to 1 dest  */
    }
    return src_len / 2;
}

#ifdef G711_TEST

#include <ipc_core.h>
s32 main(s32 argc, pv8 argv[])
{
    v8 src[100] = {0};
    v8 dest[200] = {0};
    s32 len = sizeof(src);
    ITER_INIT(iter, 2);
    while (ipc_file_read_iter (iter[0], argv[1], src, &len)
        && (len = ipc_g711u_decode(dest, src, len))
        && ipc_file_write_iter(iter[1], argv[2], dest, len));

    return 0;
}

#endif

