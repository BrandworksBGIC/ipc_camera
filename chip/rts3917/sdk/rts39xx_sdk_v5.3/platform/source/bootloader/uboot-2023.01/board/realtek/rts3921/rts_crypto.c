#include <config.h>
#include <common.h>
#include <mapmem.h>
#include <asm/io.h>
#include <linux/err.h>
#include "rts_crypto.h"
#include "bspchip.h"

#define get_val(addr)		REG32(addr)
#define set_val(addr, val)	REG32(addr) = (val)
#define clear_bit(addr, val)	set_val((addr), (get_val(addr) & ~(val)))
#define set_bit(addr, val)	set_val((addr), (get_val(addr) | (val)))

struct rlx_crypto_ctx ctx;

static unsigned int rlx_crypto_read(unsigned int reg)
{
	return get_val(0x18600000UL + reg);
}

static int rlx_crypto_write(unsigned int reg, unsigned int value)
{
	set_val(0x18600000UL + reg, value);

	return 0;
}

static int rlx_crypto_do(void)
{
	int ret = 0;
	u32 val;

	/* start crypto */
	val = rlx_crypto_read(RLX_REG_CIPHER_CTL);
	val &= ~0x3;
	val |= 0x1;
	rlx_crypto_write(RLX_REG_CIPHER_CTL, val);

	do {
		val = rlx_crypto_read(RLX_REG_CIPHER_INT_FLAG);
		val &= 0xf;
		if (val) {
			rlx_crypto_write(RLX_REG_CIPHER_INT_FLAG, val);
			break;
		}
	} while (1);

	if (val & 0xe) {
		printf("int report error:0x%x, retry\n", val & 0xe);
		ret = -EAGAIN;
		goto do_out;
	}

	/* crypto status */
	val = rlx_crypto_read(RLX_REG_CIPHER_STS);
	if ((val & 0x1) == 0) {
		printf("cipher status error, retry\n");
		ret = -EAGAIN;
		goto do_out;
	}

do_out:
	/* stop crypto */
	val = rlx_crypto_read(RLX_REG_CIPHER_CTL);
	val &= ~0x3;
	val |= 0x2;
	rlx_crypto_write(RLX_REG_CIPHER_CTL, val);

	return ret;
}

static int otp_readiv_hw(void)
{
	u16 length = 36;
	u16 group = 7;
	u32 *otp_data_addr = (u32 *)OTP_READ_DATA_0;

	rts_otp_load(group, length);
	for (int i = 0; i < 4; i++) {
		rlx_crypto_write(RLX_REG_IV_IN_DATA0 + 4 * i,
			cpu_to_be32(*otp_data_addr));
		otp_data_addr++;
	}
	return 0;
}

unsigned int read_flag_from_otp(void)
{
	//read group0, bit 95/223
	u16 group = 0;
	u16 *otp_data_addr = (u16 *)OTP_CONTROL_DATA_0;
	u16 flag;

	rts_otp_load(group, 16);
	flag = *(otp_data_addr + 5);

	return (flag & 0x8000);
}

static int rlx_crypto(u8 *dst, u8 *src, unsigned int nbytes,
		      unsigned int mask)
{
	u32 val, reg;
	int ret = 0;
	u32 *buf;
	u32 *reg_val;
	unsigned int align_mask;
	unsigned int *table_in_ptr = (unsigned int *)ctx.table_in;
	unsigned int *table_out_ptr = (unsigned int *)ctx.table_out;

	flush_cache((ulong)src, ALIGN((ulong)nbytes, ARCH_DMA_MINALIGN));

	set_bit(CRYPTO_CLOCK, CLK_ENABLE_DISABLE);
	mdelay(1);

	rlx_crypto_write(RLX_REG_CIPHER_INT_FLAG, 0xf);
	memset(&ctx, 0, sizeof(ctx));
	ctx.keylen = 16;
#ifdef CONFIG_AES_256
	ctx.keylen = 32;
#endif
	ctx.mask = mask;

	val = 0;
	align_mask = 0x7;
	if (mask & FLAGS_AES) {
		if (ctx.keylen == 16)
			val = val | ((u32)0x0 << RLX_AES_MODE_SEL);
		else if (ctx.keylen == 24)
			val = val | ((u32)0x1 << RLX_AES_MODE_SEL);
		else if (ctx.keylen == 32)
			val = val | ((u32)0x2 << RLX_AES_MODE_SEL);
		align_mask = 0xf;
	}

	if (nbytes & align_mask) {
		ret = -EINVAL;
		printf("cryto data not aligned!");
		goto out;
	}

	if (mask & FLAGS_ECB)
		val = val | ((u32)0x0 << RLX_OPERATION_MODE);
	else if (mask & FLAGS_CBC)
		val = val | ((u32)0x1 << RLX_OPERATION_MODE);
	else if (mask & FLAGS_CTR)
		val = val | ((u32)0x5 << RLX_OPERATION_MODE);

	if (mask & FLAGS_ECB)
		ctx.ivflag = 0;
	else
		ctx.ivflag = 1;

	if (mask & FLAGS_AES)
		ctx.ivlen = 16;
	else
		ctx.ivlen = 8;

	if (mask & (FLAGS_ECB | FLAGS_CBC))
		val = val | ((u32)0x1 << RLX_PADDING_SEL);
	else
		val = val | ((u32)0x0 << RLX_PADDING_SEL);

	if (mask & FLAGS_ENCRYPT)
		val = val | ((u32)0x0 << RLX_ENCTYPT_DECTYPT_SEL);
	else if (mask & FLAGS_DECRYPT)
		val = val | ((u32)0x1 << RLX_ENCTYPT_DECTYPT_SEL);

	rlx_crypto_write(RLX_REG_CIPHER_CTL, val);

	/* key */
	reg = read_flag_from_otp();
	if (reg) {
		load_key_from_sd();
		buf = map_sysmem(CONFIG_SYS_LOAD_ADDR, ctx.keylen);
		memcpy(ctx.key, buf, ctx.keylen);
		reg_val = (u32 *)ctx.key;
		rlx_crypto_write(RLX_REG_KEY_DATA0, cpu_to_be32(reg_val[0]));
		rlx_crypto_write(RLX_REG_KEY_DATA1, cpu_to_be32(reg_val[1]));
		if (ctx.keylen > 8) {
			rlx_crypto_write(RLX_REG_KEY_DATA2,
					cpu_to_be32(reg_val[2]));
			rlx_crypto_write(RLX_REG_KEY_DATA3,
					cpu_to_be32(reg_val[3]));
		}
		if (ctx.keylen > 16) {
			rlx_crypto_write(RLX_REG_KEY_DATA4,
					cpu_to_be32(reg_val[4]));
			rlx_crypto_write(RLX_REG_KEY_DATA5,
					cpu_to_be32(reg_val[5]));
		}
		if (ctx.keylen > 24) {
			rlx_crypto_write(RLX_REG_KEY_DATA6,
					cpu_to_be32(reg_val[6]));
			rlx_crypto_write(RLX_REG_KEY_DATA7,
					cpu_to_be32(reg_val[7]));
		}
#ifdef CONFIG_CBC_MODE
		load_iv_from_sd();
		buf = map_sysmem(CONFIG_SYS_LOAD_ADDR, ctx.ivlen);
		memcpy(ctx.iv, buf, ctx.ivlen);
		reg_val = (u32 *)ctx.iv;
		rlx_crypto_write(RLX_REG_IV_IN_DATA0, cpu_to_be32(reg_val[0]));
		rlx_crypto_write(RLX_REG_IV_IN_DATA1, cpu_to_be32(reg_val[1]));
		if (ctx.ivlen > 8) {
			rlx_crypto_write(RLX_REG_IV_IN_DATA2,
					cpu_to_be32(reg_val[2]));
			rlx_crypto_write(RLX_REG_IV_IN_DATA3,
					cpu_to_be32(reg_val[3]));
		}
#endif
		rlx_crypto_write(RLX_REG_CIPHER_KEY, 0);
	} else {
		rts_otp_load_aes_key(1, 36);

#ifndef CONFIG_AES_256
		rlx_crypto_write(RLX_REG_CIPHER_KEY, 1);
#else
		rlx_crypto_write(RLX_REG_CIPHER_KEY, 3);
#endif

#ifdef CONFIG_CBC_MODE
	/* get iv from otp Group7 */
	otp_readiv_hw();
#endif
	}
	/* data in & out */
	rlx_crypto_write(RLX_REG_IN_TABLE_ADDR,
		virt_to_phys(ctx.table_in));
	rlx_crypto_write(RLX_REG_OUT_TABLE_ADDR,
		virt_to_phys(ctx.table_out));

	*(table_in_ptr++) = virt_to_phys(src);
	*(table_in_ptr++) = nbytes;
	flush_cache((ulong)ctx.table_in, ALIGN((ulong)8, ARCH_DMA_MINALIGN));


	rlx_crypto_write(RLX_REG_IN_BUF_NUM, 1);

	*(table_out_ptr++) = virt_to_phys(dst);
	*(table_out_ptr++) = nbytes;
	flush_cache((ulong)ctx.table_out, ALIGN((ulong)8, ARCH_DMA_MINALIGN));

	rlx_crypto_write(RLX_REG_OUT_BUF_NUM, 1);
	rlx_crypto_write(RLX_REG_DATA_IN_LENGTH, nbytes);

	ret = rlx_crypto_do();

	invalidate_dcache_range((ulong)dst,
		ALIGN((ulong)dst + nbytes, ARCH_DMA_MINALIGN));

out:
	clear_bit(CRYPTO_CLOCK, CLK_ENABLE_DISABLE);

	return ret;
}

int crypto_init(void)
{
	set_bit(CRYPTO_FORCE_RESET, FORCE_CIPHER_RST);
	clear_bit(CRYPTO_FORCE_RESET, FORCE_CIPHER_RST);
	udelay(2);

	return 0;
}

/* aes */
int rlx_aes_ecb_encrypt(u8 *dst, u8 *src, unsigned int nbytes)
{
	int ret;
	unsigned int mask;

	mask = FLAGS_ENCRYPT | FLAGS_AES;
#ifdef CONFIG_CBC_MODE
	mask |= FLAGS_CBC;
#else
	mask |= FLAGS_ECB;
#endif
	do {
		ret = rlx_crypto(dst, src, nbytes, mask);
	} while (ret == -EAGAIN);

	return ret;
}

int rlx_aes_ecb_decrypt(u8 *dst, u8 *src, unsigned int nbytes)
{
	int ret;
	unsigned int mask;

	mask = FLAGS_DECRYPT | FLAGS_AES;
#ifdef CONFIG_CBC_MODE
	mask |= FLAGS_CBC;
#else
	mask |= FLAGS_ECB;
#endif
	do {
		ret = rlx_crypto(dst, src, nbytes, mask);
	} while (ret == -EAGAIN);

	return ret;
}
