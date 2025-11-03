#ifndef _RTS_CRYTO_H_
#define _RTS_CRYTO_H_

#define RLX_REG_KEY_DATA0		0x00
#define RLX_REG_KEY_DATA1		0x04
#define RLX_REG_KEY_DATA2		0x08
#define RLX_REG_KEY_DATA3		0x0C
#define RLX_REG_KEY_DATA4		0x10
#define RLX_REG_KEY_DATA5		0x14
#define RLX_REG_KEY_DATA6		0x18
#define RLX_REG_KEY_DATA7		0x1C
#define RLX_REG_IV_IN_DATA0		0x20
#define RLX_REG_IV_IN_DATA1		0x24
#define RLX_REG_IV_IN_DATA2		0x28
#define RLX_REG_IV_IN_DATA3		0x2C
#define RLX_REG_IV_OUT_DATA0		0x30
#define RLX_REG_IV_OUT_DATA1		0x34
#define RLX_REG_IV_OUT_DATA2		0x38
#define RLX_REG_IV_OUT_DATA3		0x3C
#define RLX_REG_IN_TABLE_ADDR		0x40
#define RLX_REG_IN_BUF_NUM		0x44
#define RLX_REG_OUT_TABLE_ADDR	        0x48
#define RLX_REG_OUT_BUF_NUM		0x4C
#define RLX_REG_CIPHER_CTL		0x50
#define RLX_REG_CIPHER_INT_EN		0x54
#define RLX_REG_CIPHER_INT_FLAG	        0x58
#define RLX_REG_CIPHER_STS		0x5C
#define RLX_REG_FINISH_BLOCK_NUM	0x60
#define RLX_REG_DATA_IN_LENGTH	        0x64
#define RLX_REG_CIPHER_KEY		0x200

#define CRYPTO_CLOCK			0x188d0020
#define CLK_ENABLE_DISABLE		BIT(24)

#define CRYPTO_FORCE_RESET		0x188d8008
#define FORCE_CIPHER_RST		BIT(4)

#define AES_MIN_KEY_SIZE	16
#define AES_MAX_KEY_SIZE	32

#define RLX_AES_MODE_SEL		10
#define RLX_OPERATION_MODE		5
#define RLX_PADDING_SEL			4
#define RLX_ENCTYPT_DECTYPT_SEL		3
#define RLX_BUSY_FLAG			2
#define RLX_STOP_CIPHER			1
#define RLX_START_CIPHER		0

#define AES_BLOCK_SIZE			16
#define DES_BLOCK_SIZE			8

#define BIT(x)			(1 << (x))

#define FLAGS_ENCRYPT		BIT(0)
#define FLAGS_DECRYPT		BIT(1)
#define FLAGS_AES		BIT(2)
#define FLAGS_ECB		BIT(5)
#define FLAGS_CBC		BIT(6)
#define FLAGS_CBCCS1		BIT(7)
#define FLAGS_CBCCS2		BIT(8)
#define FLAGS_CBCCS3		BIT(9)
#define FLAGS_CTR		BIT(10)

#define RLX_DMA_TABLE_SIZE	256
#define RLX_AES_MIN_KEY_SIZE	AES_MIN_KEY_SIZE
#define RLX_KEY_LENGTH		32
#define RLX_AES_MAX_KEY_SIZE    AES_MAX_KEY_SIZE
#define RLX_IV_LENGTH		16



struct rlx_crypto_ctx {
	u8 key[RLX_KEY_LENGTH];
	unsigned int keylen;
	u8 iv[RLX_IV_LENGTH];
	int ivflag;
	unsigned int ivlen;
	unsigned int mask;
	u32 table_in[2]
		__attribute__((aligned(ARCH_DMA_MINALIGN)));
	u32 table_out[2]
		__attribute__((aligned(ARCH_DMA_MINALIGN)));
	unsigned long size;
};

int rlx_aes_ecb_decrypt(u8 *dst, u8 *src, unsigned int nbytes);
int rlx_aes_ecb_encrypt(u8 *dst, u8 *src, unsigned int nbytes);
int rts_otp_load(u16 group, u16 length);
int rts_otp_load_aes_key(u16 group, u16 length);

#endif

