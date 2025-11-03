#include <openssl/opensslconf.h>

#include <stdio.h>
#include <string.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <linux/if_alg.h>
#include <fcntl.h>
#include <unistd.h>
#include "internal/sockets.h"

#ifndef OPENSSL_NO_HW_RTS

#define LOG_LV_DBG	4
#define LOG_LV_INFO	3
#define LOG_LV_WARN	2
#define LOG_LV_ERR	1
#define LOG_LV_NONE	0

static int log_level = LOG_LV_NONE;

#define LOG_PRINT(lv, prefix, fmt, ...) \
	do { \
		if (lv <= log_level) \
			fprintf(stdout, "%s[%s:%d]: "fmt,\
			prefix, __func__, __LINE__, ##__VA_ARGS__);\
	} while (0)

#define LOG_DBG(fmt, ...) LOG_PRINT(LOG_LV_DBG, "dbg", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_PRINT(LOG_LV_INFO, "info", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_PRINT(LOG_LV_WARN, "warn", fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) LOG_PRINT(LOG_LV_ERR, "err", fmt, ##__VA_ARGS__)

//#define debug
#ifdef debug
#define LOG_ARRAY(a, len, prefix) \
	do { \
		int ai = 0; \
		LOG_INFO("%s", prefix); \
		for (ai = 0; ai < len; ai++) \
			printf("%02x", ((unsigned char *)a)[ai]); \
		printf("\n"); \
	} while (0)
#else
#define LOG_ARRAY(a, len, prefix)
#endif

#define RTS_ENGINE_ALIGNMENT		32

static const char *engine_e_rts_id = "rts";
static const char *engine_e_rts_name = "RTS engine";

static int rts_cipher_nids[] = {
	NID_aes_128_ecb,
	NID_aes_128_cbc,
	NID_aes_128_ctr,
	NID_aes_128_ccm,
	NID_aes_128_gcm,

	NID_aes_192_ecb,
	NID_aes_192_cbc,
	NID_aes_192_ctr,
	NID_aes_192_ccm,
	NID_aes_192_gcm,

	NID_aes_256_ecb,
	NID_aes_256_cbc,
	NID_aes_256_ctr,
	NID_aes_256_ccm,
	NID_aes_256_gcm,

	NID_des_ecb,
	NID_des_ede_ecb,
	NID_des_cbc,
	NID_des_ede_cbc,

	NID_des_ede3_ecb,
	NID_des_ede3_cbc,
};

static int rts_digest_nids[] = {
	NID_sha256,
};

#define RTS_CIPHER_NIDS_NUMS \
	(sizeof(rts_cipher_nids) / sizeof(rts_cipher_nids[0]))

#define RTS_DIGEST_NIDS_NUMS \
	(sizeof(rts_digest_nids) / sizeof(rts_digest_nids[0]))

static int rts_cipher_nids_num = RTS_CIPHER_NIDS_NUMS;
static int rts_digest_nids_num = RTS_DIGEST_NIDS_NUMS;
static RSA_METHOD *rts_rsa_method;

#define AES_BLOCK_SIZE		16
#define AES_CTR_BLOCK_SIZE	1
#define AES_CCM_BLOCK_SIZE	1
#define AES_GCM_BLOCK_SIZE	1
#define AES_KEY_SIZE_128	16
#define AES_KEY_SIZE_192	24
#define AES_KEY_SIZE_256	32

#define DES_BLOCK_SIZE		8
#define DES_KEY_SIZE		8

#define DES_EDE3_BLOCK_SIZE	8
#define DES_EDE3_KEY_SIZE	24

#define AES_ECB_IVLEN		0
#define AES_GCM_IVLEN		12

#define AES_ECB_FLAGS	EVP_CIPH_ECB_MODE
#define AES_CBC_FLAGS	(EVP_CIPH_CBC_MODE | EVP_CIPH_ALWAYS_CALL_INIT)
#define AES_CTR_FLAGS	(EVP_CIPH_CTR_MODE | EVP_CIPH_ALWAYS_CALL_INIT)
#define AES_CCM_FLAGS	(EVP_CIPH_CCM_MODE | \
			EVP_CIPH_ALWAYS_CALL_INIT | \
			EVP_CIPH_CTRL_INIT | \
			EVP_CIPH_CUSTOM_IV | \
			EVP_CIPH_FLAG_AEAD_CIPHER | \
			EVP_CIPH_FLAG_CUSTOM_CIPHER)
#define AES_GCM_FLAGS	(EVP_CIPH_GCM_MODE | \
			EVP_CIPH_ALWAYS_CALL_INIT | \
			EVP_CIPH_CTRL_INIT | \
			EVP_CIPH_CUSTOM_IV | \
			EVP_CIPH_FLAG_AEAD_CIPHER | \
			EVP_CIPH_FLAG_CUSTOM_CIPHER)

#ifndef AF_ALG
#define AF_ALG 38
#endif
#define SOL_ALG 279

#ifndef ALG_SET_PUBKEY
#define ALG_SET_PUBKEY			6
#define ALG_SET_PUBKEY_ID		7
#define ALG_SET_KEY_ID			8

#endif

#define RSA_KEY_MAX_BYTES	(2 * 1024)

struct rts_tfm {
	int init;
	int sock_fd;
	int refcnt;
};

struct rts_cipher_data {
	struct rts_tfm tfm;
	int io_fd;
	int up_iv;
	unsigned char *align_buf;
	int align_len;
	size_t pclen; //plaintext/ciphertext length
	size_t assoclen; //add length
	int noncelen;
	int tagsize;
	const unsigned char *add;
	int tlscipher;
};

struct rts_digest_data {
	struct rts_tfm *tfm;
	int io_fd;
};

struct rts_rsa_data {
	struct rts_tfm tfm;
	int io_fd;
	unsigned char *key;
};

static EVP_CIPHER *rciphers[RTS_CIPHER_NIDS_NUMS];
static EVP_MD *rdigests[RTS_DIGEST_NIDS_NUMS];

/* increment counter (64-bit int) by 1 */
static void ctr64_inc(unsigned char *counter)
{
	int n = 8;
	unsigned char c;

	do {
		--n;
		c = counter[n];
		++c;
		counter[n] = c;

		if (c)
			return;
	} while (n);
}

static int get_sockaddr_alg(int nid, int mode, struct sockaddr_alg *sa)
{
	memset(sa, 0, sizeof(struct sockaddr_alg));
	sa->salg_family = AF_ALG;

	if (mode >= 0)
		sprintf(sa->salg_type, "skcipher");

	switch (mode) {
	case EVP_CIPH_ECB_MODE:
		sprintf(sa->salg_name, "ecb");
		break;
	case EVP_CIPH_CBC_MODE:
		sprintf(sa->salg_name, "cbc");
		break;
	case EVP_CIPH_CTR_MODE:
		sprintf(sa->salg_name, "ctr");
		break;
	case EVP_CIPH_CCM_MODE:
		sprintf(sa->salg_type, "aead");
		sprintf(sa->salg_name, "ccm(aes)");
		break;
	case EVP_CIPH_GCM_MODE:
		sprintf(sa->salg_type, "aead");
		sprintf(sa->salg_name, "gcm(aes)");
		break;
	case -1:
		break;
	default:
		LOG_ERR("not support mode\n");
		return 0;
	}

	switch (nid) {
	case NID_aes_128_ecb:
	case NID_aes_128_cbc:
	case NID_aes_128_ctr:
	case NID_aes_192_ecb:
	case NID_aes_192_cbc:
	case NID_aes_192_ctr:
	case NID_aes_256_ecb:
	case NID_aes_256_cbc:
	case NID_aes_256_ctr:
		strcat(sa->salg_name, "(aes)");
		break;
	case NID_des_ecb:
	case NID_des_ede_ecb:
	case NID_des_cbc:
	case NID_des_ede_cbc:
		strcat(sa->salg_name, "(des)");
		break;
	case NID_des_ede3_ecb:
	case NID_des_ede3_cbc:
		strcat(sa->salg_name, "(des3_ede)");
		break;
	case NID_aes_128_ccm:
	case NID_aes_192_ccm:
	case NID_aes_256_ccm:
	case NID_aes_128_gcm:
	case NID_aes_192_gcm:
	case NID_aes_256_gcm:
		break;
	case NID_sha256:
		sprintf(sa->salg_type, "hash");
		sprintf(sa->salg_name, "sha256");
		break;
	case NID_rsa:
		sprintf(sa->salg_type, "akcipher");
		sprintf(sa->salg_name, "rsa");
		break;
	default:
		LOG_ERR("not support nid\n");
		return 0;
	}

	return 1;
}

static void release_tfm(struct rts_tfm *tfm)
{
	if (tfm->sock_fd >= 0) {
		close(tfm->sock_fd);
		tfm->sock_fd = -1;
	}

	tfm->init = 0;
}

static int new_tfm(struct rts_tfm *tfm, int nid, int mode)
{
	int ret;
	struct sockaddr_alg sa;

	ret = get_sockaddr_alg(nid, mode, &sa);
	if (!ret) {
		ENGINEerr(ENGINE_F_NEW_TFM, ERR_R_INIT_FAIL);
		return ret;
	}

	/* socket */
	tfm->sock_fd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfm->sock_fd < 0) {
		LOG_ERR("socket failed\n");
		SYSerr(SYS_F_SOCKET, get_last_socket_error());
		ENGINEerr(ENGINE_F_NEW_TFM, ERR_R_INIT_FAIL);
		return 0;
	}

	/* bind */
	ret = bind(tfm->sock_fd, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0) {
		LOG_ERR("bind failed, errno = %d\n", errno);
		SYSerr(SYS_F_BIND, get_last_socket_error());
		ENGINEerr(ENGINE_F_NEW_TFM, ERR_R_INIT_FAIL);
		goto err;
	}

	LOG_DBG("nid=%d, sock_fd=0x%08x\n", nid, tfm->sock_fd);
	tfm->init = 1;
	return 1;
err:
	release_tfm(tfm);
	return 0;
}

static int rts_accept(int fd)
{
	int io_fd;

	/* accept */
	io_fd = accept(fd, NULL, 0);
	if (io_fd < 0) {
		LOG_ERR("accept failed, errno = %d\n", errno);
		SYSerr(SYS_F_ACCEPT, get_last_socket_error());
		ENGINEerr(ENGINE_F_RTS_ACCEPT, ERR_R_INIT_FAIL);
		return -1;
	}

	return io_fd;
}

static void rts_unaccept(int *io_fd)
{
	if (*io_fd >= 0) {
		close(*io_fd);
		*io_fd = -1;
	}
}

static int set_key(int sock_fd,
			const unsigned char *key, int key_len,
			int pubkey)
{
	int ret;

	if (!key || !key_len) {
		LOG_ERR("key is NULL\n");
		ENGINEerr(ENGINE_F_SET_KEY, ERR_R_PASSED_NULL_PARAMETER);
		return 0;
	}

	/* set socket opt (set key) */
	ret = setsockopt(sock_fd, SOL_ALG,
				pubkey ? ALG_SET_PUBKEY : ALG_SET_KEY,
				key, key_len);
	if (ret < 0) {
		LOG_ERR("setsockopt key failed, keylen=%d, errno=%d\n",
					key_len, errno);
		SYSerr(SYS_F_SETSOCKOPT, get_last_socket_error());
		ENGINEerr(ENGINE_F_SET_KEY, ERR_R_OPERATION_FAIL);
		return 0;
	}

	return 1;
}

static int set_tag(int sock_fd, int tagsize)
{
	int ret;

	/* set socket opt (set authsize) */
	ret = setsockopt(sock_fd, SOL_ALG, ALG_SET_AEAD_AUTHSIZE,
				NULL, tagsize);
	if (ret < 0) {
		LOG_ERR("setsockopt authsize = %d err, errno = %d\n",
					tagsize, errno);
		SYSerr(SYS_F_SETSOCKOPT, get_last_socket_error());
		ENGINEerr(ENGINE_F_SET_TAG, ERR_R_OPERATION_FAIL);
		return 0;
	}

	return 1;
}

static int set_control(int io_fd, int op, size_t assoclen,
			const unsigned char *iv, int iv_len)
{
	int ret;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct af_alg_iv *af_iv;

	LOG_DBG("op=%d, assoclen=%d, iv=%d, iv_len=%d\n",
				op, assoclen, iv, iv_len);

	/* init msg */
	memset(&msg, 0, sizeof(msg));

	/* msg_control (store op & iv & assoclen) */
	msg.msg_controllen = CMSG_SPACE(sizeof(int)) +
		((!iv_len || !iv) ? 0 :
		 CMSG_SPACE(sizeof(struct af_alg_iv) + iv_len)) +
		(!assoclen ? 0 : CMSG_SPACE(sizeof(int)));
	/* must init 0, otherwise CMSG_NXTHDR may return NULL*/
	msg.msg_control = calloc(1, msg.msg_controllen);
	if (!msg.msg_control) {
		LOG_ERR("alloc msg_control fail\n");
		ENGINEerr(ENGINE_F_SET_CONTROL, ERR_R_MALLOC_FAILURE);
		return -1;
	}

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	*(int *)CMSG_DATA(cmsg) = op;

	/* iv */
	if (iv_len && iv) {
		cmsg = CMSG_NXTHDR(&msg, cmsg);
		cmsg->cmsg_len =
			CMSG_LEN(sizeof(struct af_alg_iv) + iv_len);
		cmsg->cmsg_level = SOL_ALG;
		cmsg->cmsg_type = ALG_SET_IV;
		af_iv = (struct af_alg_iv *)CMSG_DATA(cmsg);
		af_iv->ivlen = iv_len;
		memcpy(af_iv->iv, iv, iv_len);
	}

	/* add */
	if (assoclen) {
		cmsg = CMSG_NXTHDR(&msg, cmsg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_ALG;
		cmsg->cmsg_type = ALG_SET_AEAD_ASSOCLEN;
		*(unsigned int *)CMSG_DATA(cmsg) = assoclen;
	}

	/* sendmsg */
	ret = sendmsg(io_fd, &msg, MSG_MORE);
	if (ret < 0) {
		LOG_ERR("sendmsg err, errno = %d\n", errno);
		ENGINEerr(ENGINE_F_SET_CONTROL, ENGINE_R_SENDMSG_FAIL);
		ERR_add_error_data(2, "errno: ", strerror(errno));
		ret = 0;
		goto err;
	}

	ret = 1;
err:
	free(msg.msg_control);
	msg.msg_control = NULL;

	return ret;
}

static int send_data(int io_fd,
			struct iovec *iniov, int inlen,
			struct iovec *outiov, int outlen,
			int flags)
{
	int ret = 0;
	struct msghdr msg;

	LOG_DBG(
		"io_fd=0x%08x, iniov=%d, inlen=%d, outiov=%d, outlen=%d, flags=%d\n",
		io_fd, iniov, inlen, outiov, outlen, flags);

	memset(&msg, 0, sizeof(msg));

	/* sendmsg */
	msg.msg_iov = iniov;
	msg.msg_iovlen = inlen;
	ret = sendmsg(io_fd, &msg, flags);
	if (ret < 0) {
		LOG_ERR("sendmsg err, errno = %d\n", errno);
		ENGINEerr(ENGINE_F_SET_DATA, ENGINE_R_SENDMSG_FAIL);
		ERR_add_error_data(2, "errno: ", strerror(errno));
		return 0;
	}

	/* recvmsg */
	if (outiov && outlen) {
		msg.msg_iov = outiov;
		msg.msg_iovlen = outlen;

		ret = recvmsg(io_fd, &msg, 0);
		if (ret < 0) {
			LOG_ERR("read err, errno = %d\n", errno);
			ENGINEerr(ENGINE_F_SET_DATA, ENGINE_R_RECVMSG_FAIL);
			ERR_add_error_data(2, "errno: ", strerror(errno));
			return 0;
		}
	}

	return ret;
}

static int send_data_buf(int io_fd, unsigned char *out,
			const unsigned char *in, size_t inl, int flags)
{
	struct iovec iniov, outiov;
	int inlen = 0, outlen = 0;

	LOG_DBG("io_fd=%d, inl=%d\n", io_fd, inl);

	if (in) {
		iniov.iov_base = (void *)in;
		iniov.iov_len = inl;
		inlen = 1;
	}

	if (out) {
		outiov.iov_base = (void *)out;
		outiov.iov_len = inl;
		outlen = 1;
	}

	return send_data(io_fd, in ? &iniov : NULL, inlen,
				out ? &outiov : NULL, outlen, flags);
}


/* cipher */
static int rts_cipher_cleanup(EVP_CIPHER_CTX *ctx)
{
	struct rts_cipher_data *cdata;

	LOG_INFO("ctx=0x%08x\n", ctx);

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);
	if (!cdata)
		return 0;

	rts_unaccept(&cdata->io_fd);

	if (cdata->tfm.init)
		release_tfm(&cdata->tfm);

	if (cdata->align_buf) {
		free(cdata->align_buf);
		cdata->align_buf = NULL;
	}

	return 1;
}

static int rts_cipher_init(EVP_CIPHER_CTX *ctx, const unsigned char *key,
		const unsigned char *iv, int enc)
{
	struct rts_cipher_data *cdata;
	struct rts_tfm *tfm;
	int ret, nid, mode, key_len;

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);
	if (!cdata) {
		ENGINEerr(ENGINE_F_RTS_CIPHER_INIT, ERR_R_INIT_FAIL);
		return 0;
	}

	nid = EVP_CIPHER_CTX_nid(ctx);
	mode = EVP_CIPHER_CTX_mode(ctx);

	LOG_INFO("nid=%d, ctx=0x%08x, key=%d, iv=%d, enc=%d\n",
				nid, ctx, key, iv, enc);
	if (key) {
		tfm = &cdata->tfm;
		key_len = EVP_CIPHER_CTX_key_length(ctx);


		if (tfm->init) {
			rts_unaccept(&cdata->io_fd);
			release_tfm(tfm);
		}

		ret = new_tfm(tfm, nid, mode);
		if (!ret)
			goto err;

		ret = set_key(tfm->sock_fd, key, key_len, 0);
		if (!ret)
			goto err;

		if (mode == EVP_CIPH_CCM_MODE ||
			mode == EVP_CIPH_GCM_MODE) {
			ret = set_tag(tfm->sock_fd, cdata->tagsize);
			if (!ret) {
				LOG_ERR("set tag fail\n");
				goto err;
			}
		}

		cdata->io_fd = rts_accept(cdata->tfm.sock_fd);
		if (cdata->io_fd < 0)
			goto err;
	}

	if (iv) {
		cdata->up_iv = 1;

		switch (mode) {
		case EVP_CIPH_ECB_MODE:
			cdata->up_iv = 0;
			break;
		case EVP_CIPH_CCM_MODE:
			memcpy(EVP_CIPHER_CTX_iv_noconst(ctx) + 1,
						iv, cdata->noncelen);
			break;
		case EVP_CIPH_GCM_MODE:
			memcpy(EVP_CIPHER_CTX_iv_noconst(ctx),
				iv, EVP_CIPHER_CTX_iv_length(ctx));
			break;
		default:
			break;
		}
	}


	return 1;
err:
	rts_cipher_cleanup(ctx);
	ENGINEerr(ENGINE_F_RTS_CIPHER_INIT, ERR_R_INIT_FAIL);
	return 0;
}

static int align_buffer(struct rts_cipher_data *cdata,
			const unsigned char *in, int inl, int align)
{
	int mem_len;

	if (!cdata || !in || !inl)
		return 0;

	mem_len = ((inl - 1) / align + 1) * align;

	if (((unsigned int)in & (align - 1)) || (inl != mem_len)) {
		if (cdata->align_buf) {
			if (cdata->align_len >= mem_len)
				return 1;

			free(cdata->align_buf);
		}

		cdata->align_buf = (unsigned char *)
			aligned_alloc(align, mem_len);
		if (!cdata->align_buf)
			return 0;

		cdata->align_len = mem_len;
		return 1;
	}

	return 0;
}

#if 0
static int do_cipher(struct rts_tfm *tfm, unsigned char *out,
			const unsigned char *in, size_t inl,
			const unsigned char *iv, int iv_len,
			unsigned char *tag, int tagsize,
			size_t assoclen, int enc, int flags)
{
	int ret;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov[2];
	struct af_alg_iv *af_iv;

	LOG_DBG(
		"inl=%d, iv=%d, iv_len=%d, tag=%d, tagsize=%d, assoclen=%d, enc=%d, flags=%d\n",
				inl, iv, iv_len, tag, tagsize,
				assoclen, enc, flags);

	/* init msg */
	/* iovec (store crypto input text) */
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;

	/* msg_control (store op & iv & assoclen) */
	msg.msg_controllen = CMSG_SPACE(sizeof(int)) +
		((!iv_len || !iv) ? 0 :
		 CMSG_SPACE(sizeof(struct af_alg_iv) + iv_len)) +
		(!assoclen ? 0 : CMSG_SPACE(sizeof(int)));
	msg.msg_control = malloc(msg.msg_controllen);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	*(int *)CMSG_DATA(cmsg) = enc ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT;

	/* iv */
	if (iv_len && iv) {
		cmsg = CMSG_NXTHDR(&msg, cmsg);
		cmsg->cmsg_len =
			CMSG_LEN(sizeof(struct af_alg_iv) + iv_len);
		cmsg->cmsg_level = SOL_ALG;
		cmsg->cmsg_type = ALG_SET_IV;
		af_iv = (struct af_alg_iv *)CMSG_DATA(cmsg);
		af_iv->ivlen = iv_len;
		memcpy(af_iv->iv, iv, iv_len);
	}

	/* add */
	if (assoclen) {
		cmsg = CMSG_NXTHDR(&msg, cmsg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_ALG;
		cmsg->cmsg_type = ALG_SET_AEAD_ASSOCLEN;
		*(unsigned int *)CMSG_DATA(cmsg) = assoclen;
	}

	/* sendmsg */
	iov[0].iov_base = (void *)in;
	iov[0].iov_len = inl + assoclen;
	msg.msg_iovlen = 1;

	/* for aead decrypt */
	if (tag && tagsize && !enc) {
		iov[1].iov_base = tag;
		iov[1].iov_len = tagsize;
		msg.msg_iovlen = 2;
	}

	ret = sendmsg(tfm->io_fd, &msg, flags);
	if (ret < 0) {
		LOG_ERR("sendmsg err, errno = %d\n", errno);
		ret = 0;
		goto err;
	}

	/* recvmsg */
	iov[0].iov_base = out;
	iov[0].iov_len = inl + assoclen;
	msg.msg_iovlen = 1;

	/* for aead encrypt */
	if (tag && tagsize && enc) {
		iov[1].iov_base = tag;
		iov[1].iov_len = tagsize;
		msg.msg_iovlen = 2;
	}

	ret = recvmsg(tfm->io_fd, &msg, 0);
	if (ret < 0) {
		LOG_ERR("read err\n");
		ret = 0;
		goto err;
	}

	ret = 1;
err:
	free(msg.msg_control);
	msg.msg_control = NULL;

	return ret;
}
#endif

static int do_aes_ccm_tls_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		const unsigned char *in, size_t inl)
{
	struct rts_cipher_data *cdata;
	int iv_len = 0, enc, ret;
	unsigned char *iv = NULL;
	struct iovec iniov[3], outiov[3];
	int inlen = 0, outlen = 0;
	unsigned char *align_buf;

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);

	LOG_INFO("ctx=0x%08x, out=%d, in=%d, inl=%d\n",
						ctx, out, in, inl);

	/* Encrypt/decrypt must be performed in place */
	if (out != in ||
		inl < (EVP_CCM_TLS_EXPLICIT_IV_LEN + cdata->tagsize))
		return -1;
	LOG_ARRAY(in, inl, "in: 0x");

	/* If encrypting set explicit IV from sequence number (start of AAD) */
	enc = EVP_CIPHER_CTX_encrypting(ctx);
	if (enc)
		memcpy(out, cdata->add, EVP_CCM_TLS_EXPLICIT_IV_LEN);
	/* Get rest of IV from explicit IV */
	iv = EVP_CIPHER_CTX_iv_noconst(ctx);
	memcpy(iv + 1 + EVP_CCM_TLS_FIXED_IV_LEN,
				in, EVP_CCM_TLS_EXPLICIT_IV_LEN);

	/* send add */
	ret = send_data_buf(cdata->io_fd, NULL,
					cdata->add, cdata->assoclen, MSG_MORE);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_CCM_TLS_CIPHER,
					ERR_R_OPERATION_FAIL);
		return -1;
	}

	/* Fix buffer and length to point to payload */
	in += EVP_CCM_TLS_EXPLICIT_IV_LEN;
	out += EVP_CCM_TLS_EXPLICIT_IV_LEN;
	/* Correct length value */
	inl -= EVP_CCM_TLS_EXPLICIT_IV_LEN + cdata->tagsize;

	/* set control */
	iv_len = EVP_CIPHER_CTX_iv_length(ctx);
	ret = set_control(cdata->io_fd,
				enc ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT,
				cdata->assoclen, iv, iv_len);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_CCM_TLS_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	/* align out */
	if (align_buffer(cdata, out, inl, RTS_ENGINE_ALIGNMENT))
		align_buf = cdata->align_buf;
	else
		align_buf = NULL;

	/* do cipher */
	if (enc) {
		/* in ptext */
		iniov[0].iov_base = (unsigned char *)in;
		iniov[0].iov_len = inl;
		inlen = 1;
		/* out add */
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		/* out ctext */
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = inl;
		/* out tag */
		outiov[2].iov_base = out + inl;
		outiov[2].iov_len = cdata->tagsize;
		outlen = 3;
	} else {
		/* in ctext + tag */
		iniov[0].iov_base = (unsigned char *)in;
		iniov[0].iov_len = inl + cdata->tagsize;
		inlen = 1;
		/* out add */
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		/* out ptext */
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = inl;
		outlen = 2;
	}

	ret = send_data(cdata->io_fd, iniov, inlen, outiov, outlen, 0);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_CCM_TLS_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	if (align_buf)
		memcpy(out, align_buf, inl);

	if (enc)
		inl += (EVP_CCM_TLS_EXPLICIT_IV_LEN + cdata->tagsize);
	LOG_ARRAY(out, inl, "out: 0x");
	return inl;
}

static int do_aes_ccm_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		const unsigned char *in, size_t inl)
{
	struct rts_cipher_data *cdata;
	int iv_len = 0, enc, ret;
	const unsigned char *iv = NULL;
	unsigned char *tag = NULL;
	struct iovec iniov[3], outiov[3];
	int inlen = 0, outlen = 0;
	unsigned char *align_buf;

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);

	/* for tls */
	if (cdata->tlscipher)
		return do_aes_ccm_tls_cipher(ctx, out, in, inl);

	LOG_INFO("out=%d, in=%d, inl=%d\n", out, in, inl);

	/* EVP_*Final() doesn't return any data */
	if (in == NULL && out != NULL)
		return 0;

	if (!out) {
		/* set plaintext/ciphertest length */
		if (!in) {
			cdata->pclen = inl;
			return inl;
		}

		/* If have AAD need message length */
		if (inl && !cdata->pclen) {
			ENGINEerr(ENGINE_F_DO_AES_CCM_CIPHER,
						ENGINE_R_NOT_INITIALISED);
			return -1;
		}

		/* set add length */
		cdata->assoclen = inl;
		cdata->add = in;

		/* send add */
		ret = send_data_buf(cdata->io_fd, NULL, in, inl, MSG_MORE);
		if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_CCM_CIPHER,
						ERR_R_OPERATION_FAIL);
			return -1;
		}

		return inl;
	}

	enc = EVP_CIPHER_CTX_encrypting(ctx);
	tag = EVP_CIPHER_CTX_buf_noconst(ctx);

	if (cdata->up_iv) {
		iv = EVP_CIPHER_CTX_iv(ctx);
		iv_len = EVP_CIPHER_CTX_iv_length(ctx);
		cdata->up_iv = 0;
	}

	/* do cipher */
	/* set control */
	ret = set_control(cdata->io_fd,
				enc ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT,
				cdata->assoclen, iv, iv_len);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_CCM_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	/* align out */
	if (align_buffer(cdata, out, inl, RTS_ENGINE_ALIGNMENT))
		align_buf = cdata->align_buf;
	else
		align_buf = NULL;

	/* send p/ctext */
	if (enc) {
		iniov[0].iov_base = (void *)in;
		iniov[0].iov_len = inl;
		inlen = 1;
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = inl;
		outiov[2].iov_base = tag;
		outiov[2].iov_len = cdata->tagsize;
		outlen = 3;

	} else {
		iniov[0].iov_base = (void *)in;
		iniov[0].iov_len = inl;
		iniov[1].iov_base = tag;
		iniov[1].iov_len = cdata->tagsize;
		inlen = 2;
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = inl;
		outlen = 2;
	}

	ret = send_data(cdata->io_fd, iniov, inlen, outiov, outlen, 0);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_CCM_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	if (align_buf)
		memcpy(out, align_buf, inl);

	return inl;
}

static int do_aes_gcm_tls_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		const unsigned char *in, size_t inl)
{
	struct rts_cipher_data *cdata;
	int iv_len = 0, enc, ret;
	const unsigned char *iv = NULL;
	struct iovec iniov[3], outiov[3];
	int inlen = 0, outlen = 0;
	unsigned char *align_buf;

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);

	LOG_INFO("ctx=0x%08x, out=%d, in=%d, inl=%d\n",
						ctx, out, in, inl);

	/* Encrypt/decrypt must be performed in place */
	if (out != in ||
		inl < (EVP_GCM_TLS_EXPLICIT_IV_LEN + cdata->tagsize))
		return -1;
	LOG_ARRAY(in, inl, "in: 0x");

	/*
	 * Set IV from start of buffer or generate IV and write to start of
	 * buffer.
	 */
	enc = EVP_CIPHER_CTX_encrypting(ctx);
	if (EVP_CIPHER_CTX_ctrl(ctx, enc ?
				EVP_CTRL_GCM_IV_GEN :
				EVP_CTRL_GCM_SET_IV_INV,
				EVP_GCM_TLS_EXPLICIT_IV_LEN, out) <= 0) {
		ENGINEerr(ENGINE_F_DO_AES_GCM_TLS_CIPHER,
						ERR_R_OPERATION_FAIL);
		return -1;
	}

	/* send add */
	ret = send_data_buf(cdata->io_fd, NULL,
					cdata->add, cdata->assoclen, MSG_MORE);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_GCM_TLS_CIPHER,
					ERR_R_OPERATION_FAIL);
		return -1;
	}

	/* Fix buffer and length to point to payload */
	in += EVP_GCM_TLS_EXPLICIT_IV_LEN;
	out += EVP_GCM_TLS_EXPLICIT_IV_LEN;
	inl -= EVP_GCM_TLS_EXPLICIT_IV_LEN + cdata->tagsize;

	/* set control */
	iv = EVP_CIPHER_CTX_iv(ctx);
	iv_len = EVP_CIPHER_CTX_iv_length(ctx);

	ret = set_control(cdata->io_fd,
				enc ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT,
				cdata->assoclen, iv, iv_len);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_GCM_TLS_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	/* align out */
	if (align_buffer(cdata, out, inl, RTS_ENGINE_ALIGNMENT))
		align_buf = cdata->align_buf;
	else
		align_buf = NULL;

	/* do cipher */
	if (enc) {
		/* in ptext */
		iniov[0].iov_base = (unsigned char *)in;
		iniov[0].iov_len = inl;
		inlen = 1;
		/* out add */
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		/* out ctext */
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = inl;
		/* out tag */
		outiov[2].iov_base = out + inl;
		outiov[2].iov_len = cdata->tagsize;
		outlen = 3;
	} else {
		/* in ctext + tag */
		iniov[0].iov_base = (unsigned char *)in;
		iniov[0].iov_len = inl + cdata->tagsize;
		inlen = 1;
		/* out add */
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		/* out ptext */
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = inl;
		outlen = 2;
	}

	ret = send_data(cdata->io_fd, iniov, inlen, outiov, outlen, 0);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_GCM_TLS_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	if (align_buf)
		memcpy(out, align_buf, inl);

	if (enc)
		inl += (EVP_GCM_TLS_EXPLICIT_IV_LEN + cdata->tagsize);
	LOG_ARRAY(out, inl, "out: 0x");
	return inl;
}

static int do_aes_gcm_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		const unsigned char *in, size_t inl)
{
	struct rts_cipher_data *cdata;
	int iv_len = 0, enc, ret;
	const unsigned char *iv = NULL;
	unsigned char *tag = NULL;
	struct iovec iniov[3], outiov[3];
	int inlen = 0, outlen = 0;
	unsigned char *align_buf;
	size_t pclen;

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);

	/* for tls */
	if (cdata->tlscipher)
		return do_aes_gcm_tls_cipher(ctx, out, in, inl);

	LOG_INFO("ctx=0x%08x, out=%d, in=%d, inl=%d\n",
						ctx, out, in, inl);

	if (in) {
		if (!out) {
			/* set add length */
			cdata->assoclen += inl;
			cdata->add = in;

			/* send add */
			ret = send_data_buf(cdata->io_fd, NULL,
						in, inl, MSG_MORE);
			if (!ret) {
				ENGINEerr(ENGINE_F_DO_AES_GCM_CIPHER,
						ERR_R_OPERATION_FAIL);
				return -1;
			}

			return inl;
		}

		/* set p/ctext length */
		cdata->pclen += inl;

		/* send p/ctext */
		ret = send_data_buf(cdata->io_fd, NULL,
						in, inl, MSG_MORE);
		if (!ret) {
			ENGINEerr(ENGINE_F_DO_AES_GCM_CIPHER,
						ERR_R_OPERATION_FAIL);
			return -1;
		}

		return 0;
	}

	/* EVP_*Final() */
	if (!out) {
		ENGINEerr(ENGINE_F_DO_AES_GCM_CIPHER,
					ENGINE_R_INVALID_ARGUMENT);
		return -1;
	}

	enc = EVP_CIPHER_CTX_encrypting(ctx);
	tag = EVP_CIPHER_CTX_buf_noconst(ctx);

	if (cdata->up_iv) {
		iv = EVP_CIPHER_CTX_iv(ctx);
		iv_len = EVP_CIPHER_CTX_iv_length(ctx);
		cdata->up_iv = 0;
	}

	/* do cipher */
	/* set control */
	ret = set_control(cdata->io_fd,
				enc ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT,
				cdata->assoclen, iv, iv_len);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_GCM_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	/* align out */
	if (align_buffer(cdata, out, cdata->pclen, RTS_ENGINE_ALIGNMENT))
		align_buf = cdata->align_buf;
	else
		align_buf = NULL;

	/* send p/ctext */
	if (enc) {
		inlen = 0;
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = cdata->pclen;
		outiov[2].iov_base = tag;
		outiov[2].iov_len = cdata->tagsize;
		outlen = 3;
	} else {
		iniov[0].iov_base = tag;
		iniov[0].iov_len = cdata->tagsize;
		inlen = 1;
		outiov[0].iov_base = (void *)cdata->add;
		outiov[0].iov_len = cdata->assoclen;
		outiov[1].iov_base = (void *)align_buf ? align_buf : out;
		outiov[1].iov_len = cdata->pclen;
		outlen = 2;
	}

	ret = send_data(cdata->io_fd, iniov, inlen, outiov, outlen, 0);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_AES_GCM_CIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	if (align_buf)
		memcpy(out, align_buf, cdata->pclen);

	pclen = cdata->pclen;
	cdata->pclen = 0;
	cdata->assoclen = 0;

	return pclen;
}

static int do_skcipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		const unsigned char *in, size_t inl)
{
	struct rts_cipher_data *cdata;
	int iv_len = 0, enc, ret;
	const unsigned char *iv = NULL;
	unsigned char *align_buf;

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);
	enc = EVP_CIPHER_CTX_encrypting(ctx);

	if (cdata->up_iv) {
		iv = EVP_CIPHER_CTX_iv(ctx);
		iv_len = EVP_CIPHER_CTX_iv_length(ctx);
		cdata->up_iv = 0;
	}

	/* do cipher */
	/* set control */
	ret = set_control(cdata->io_fd,
				enc ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT,
				0, iv, iv_len);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_SKCIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	/* aling out */
	if (align_buffer(cdata, out, inl, RTS_ENGINE_ALIGNMENT))
		align_buf = cdata->align_buf;
	else
		align_buf = NULL;

	/* send p/ctext */
	ret = send_data_buf(cdata->io_fd,
			align_buf ? align_buf : out, in, inl, 0);
	if (!ret) {
		ENGINEerr(ENGINE_F_DO_SKCIPHER,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	if (align_buf)
		memcpy(out, align_buf, inl);

	return 1;
}


static int rts_do_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		const unsigned char *in, size_t inl)
{
	int mem_len, mode, ret;

	mode = EVP_CIPHER_CTX_mode(ctx);

	/* do cipher */
	switch (mode) {
	case EVP_CIPH_CCM_MODE:
		ret = do_aes_ccm_cipher(ctx, out, in, inl);
		break;
	case EVP_CIPH_GCM_MODE:
		ret = do_aes_gcm_cipher(ctx, out, in, inl);
		break;
	default:
		ret = do_skcipher(ctx, out, in, inl);
		break;
	}

	return ret;
}

static int rts_aes_ccm_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
	struct rts_cipher_data *cdata;

	LOG_INFO("ctx=0x%08x, type=%d, arg=%d\n", ctx, type, arg);
	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);

	switch (type) {
	case EVP_CTRL_INIT:
		cdata->tlscipher = 0;
		cdata->pclen = 0;
		cdata->assoclen = 0;
		cdata->tagsize = 12;

		unsigned char *iv = EVP_CIPHER_CTX_iv_noconst(ctx);

		cdata->noncelen = 7;
		iv[0] = 15 - 7 - 1;
		break;
	case EVP_CTRL_GET_IVLEN:
		*(int *)ptr = cdata->noncelen;
		break;
	case EVP_CTRL_CCM_SET_L:
		arg = 15 - arg;
	case EVP_CTRL_AEAD_SET_IVLEN: {
		//n:7,8,9,10,11,12,13
		if (arg < 7 || arg > 13) {
			ENGINEerr(ENGINE_F_RTS_AES_CCM_CTRL,
					ENGINE_R_INVALID_IVLEN);
			return 0;
		}

		unsigned char *iv = EVP_CIPHER_CTX_iv_noconst(ctx);

		cdata->noncelen = arg;
		iv[0] = 15 - arg - 1;
		break;
	}
	case EVP_CTRL_AEAD_SET_TAG:
		//tag:4,6,8,10,12,14,16
		if ((arg & 1) || arg < 4 || arg > 16) {
			ENGINEerr(ENGINE_F_RTS_AES_CCM_CTRL,
					ENGINE_R_INVALID_TAGSIZE);
			return 0;
		}

		if (EVP_CIPHER_CTX_encrypting(ctx) && ptr) {
			ENGINEerr(ENGINE_F_RTS_AES_CCM_CTRL,
					ERR_R_PASSED_NULL_PARAMETER);
			return 0;
		}

		if (ptr)
			memcpy(EVP_CIPHER_CTX_buf_noconst(ctx), ptr, arg);

		cdata->tagsize = arg;
		break;
	case EVP_CTRL_AEAD_GET_TAG:
		memcpy(ptr, EVP_CIPHER_CTX_buf_noconst(ctx), arg);
		break;
	case EVP_CTRL_AEAD_SET_IV_FIXED:
		/* Sanity check length */
		if (arg != EVP_CCM_TLS_FIXED_IV_LEN)
			return 0;
		/* Just copy tho first part of IV */
		memcpy(EVP_CIPHER_CTX_iv_noconst(ctx) + 1, ptr, arg);
		break;
	case EVP_CTRL_AEAD_TLS1_AAD:
		/* Save the AAD for later use */
		if (arg != EVP_AEAD_TLS1_AAD_LEN)
			return 0;

		unsigned char *buf = EVP_CIPHER_CTX_buf_noconst(ctx);

		cdata->assoclen = arg;
		cdata->add = buf;
		memcpy(buf, ptr, arg);
		LOG_ARRAY(buf, arg, "add pre: 0x");

		unsigned int len = buf[arg - 2] << 8 | buf[arg - 1];
		/* Correct length for explicit IV */
		if (len < EVP_CCM_TLS_EXPLICIT_IV_LEN)
			return 0;
		len -= EVP_CCM_TLS_EXPLICIT_IV_LEN;

		/* If decrypting correct for tag too */
		if (!EVP_CIPHER_CTX_encrypting(ctx)) {
			if (len < cdata->tagsize)
				return 0;
			len -= cdata->tagsize;
		}

		buf[arg - 2] = len >> 8;
		buf[arg - 1] = len & 0xff;
		LOG_ARRAY(buf, arg, "add end: 0x");

		cdata->tlscipher = 1;
		/* Extra padding: tag appended to record */
		return cdata->tagsize;
	default:
		return -1;
	}

	return 1;
}

static int rts_aes_gcm_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
	struct rts_cipher_data *cdata;
	EVP_CIPHER *cipher;
	unsigned char *iv = EVP_CIPHER_CTX_iv_noconst(ctx);
	int ivlen = EVP_CIPHER_CTX_iv_length(ctx);

	LOG_INFO("ctx=0x%08x, type=%d, arg=%d\n", ctx, type, arg);

	cdata = EVP_CIPHER_CTX_get_cipher_data(ctx);
	cipher = (EVP_CIPHER *)EVP_CIPHER_CTX_cipher(ctx);

	switch (type) {
	case EVP_CTRL_INIT:
		cdata->tlscipher = 0;
		cdata->pclen = 0;
		cdata->assoclen = 0;
		cdata->tagsize = 16;
		break;
	case EVP_CTRL_GET_IVLEN:
		*(int *)ptr = EVP_CIPHER_iv_length(cipher);
		break;
	case EVP_CTRL_AEAD_SET_IVLEN: {
		if (arg <= 0 || arg > 16) {
			ENGINEerr(ENGINE_F_RTS_AES_GCM_CTRL,
					ENGINE_R_INVALID_IVLEN);
			return 0;
		}

		EVP_CIPHER_meth_set_iv_length(cipher, arg);
		break;
	}
	case EVP_CTRL_AEAD_SET_TAG:
		LOG_DBG("set tag=%d\n", arg);

		//tag:4,8,12,13,14,15,16
		if (arg > 16 || (arg < 12 && arg != 4 && arg != 8)) {
			ENGINEerr(ENGINE_F_RTS_AES_GCM_CTRL,
					ENGINE_R_INVALID_TAGSIZE);
			return 0;
		}

//		if (EVP_CIPHER_CTX_encrypting(ctx))
//			return 0;

		if (ptr)
			memcpy(EVP_CIPHER_CTX_buf_noconst(ctx), ptr, arg);

		cdata->tagsize = arg;
		break;
	case EVP_CTRL_AEAD_GET_TAG:
		if (arg > 16 || (arg < 12 && arg != 4 && arg != 8)) {
			ENGINEerr(ENGINE_F_RTS_AES_GCM_CTRL,
					ENGINE_R_INVALID_TAGSIZE);
			return 0;
		}

		memcpy(ptr, EVP_CIPHER_CTX_buf_noconst(ctx), arg);
		break;
	case EVP_CTRL_AEAD_SET_IV_FIXED:
		/* Special case: -1 length restores whole IV */
		if (arg == -1) {
			memcpy(iv, ptr, ivlen);
			return 1;
		}

		/*
		 * Fixed field must be at least 4 bytes and invocation
		 * field at least 8.
		 */
		if ((arg < 4) || (ivlen - arg) < 8)
			return 0;

		if (arg)
			memcpy(iv, ptr, arg);
		LOG_ARRAY(ptr, arg, "iv fixed: 0x");

		if (EVP_CIPHER_CTX_encrypting(ctx) &&
			RAND_bytes(iv + arg, ivlen - arg) <= 0)
			return 0;
		LOG_ARRAY(iv, ivlen, "iv: 0x");
		break;
	case EVP_CTRL_GCM_IV_GEN:
		if (arg <= 0 || arg > ivlen)
			arg = ivlen;
		/*
		 * Invocation field will be at least 8 bytes in size
		 * and so no need to check wrap around or increment
		 * more than last 8 bytes.
		 */
		ctr64_inc(iv + ivlen - 8);
		memcpy(ptr, iv + ivlen - arg, arg);
		LOG_ARRAY(ptr, arg, "iv gen: 0x");
		break;
	case EVP_CTRL_GCM_SET_IV_INV:
		if (EVP_CIPHER_CTX_encrypting(ctx))
			return 0;
		memcpy(iv + ivlen - arg, ptr, arg);
		LOG_ARRAY(ptr, arg, "iv inv: 0x");
		break;
	case EVP_CTRL_AEAD_TLS1_AAD:
		/* Save the AAD for later use */
		if (arg != EVP_AEAD_TLS1_AAD_LEN)
			return 0;

		unsigned char *buf = EVP_CIPHER_CTX_buf_noconst(ctx);

		cdata->assoclen = arg;
		cdata->add = buf;
		memcpy(buf, ptr, arg);
		LOG_ARRAY(buf, arg, "add pre: 0x");

		unsigned int len = buf[arg - 2] << 8 | buf[arg - 1];
		/* Correct length for explicit IV */
		if (len < EVP_GCM_TLS_EXPLICIT_IV_LEN)
			return 0;
		len -= EVP_GCM_TLS_EXPLICIT_IV_LEN;

		/* If decrypting correct for tag too */
		if (!EVP_CIPHER_CTX_encrypting(ctx)) {
			if (len < EVP_GCM_TLS_TAG_LEN)
				return 0;
			len -= EVP_GCM_TLS_TAG_LEN;
		}

		buf[arg - 2] = len >> 8;
		buf[arg - 1] = len & 0xff;
		LOG_ARRAY(buf, arg, "add end: 0x");

		cdata->tlscipher = 1;
		cdata->tagsize = EVP_GCM_TLS_TAG_LEN;
		/* Extra padding: tag appended to record */
		return EVP_GCM_TLS_TAG_LEN;
	default:
		return -1;
	}

	return 1;
}

static EVP_CIPHER *cipher_new(int nid, int block_size, int iv_len,
				int key_len, unsigned long flags,
				int (*ctrl)(EVP_CIPHER_CTX *, int type,
					int arg, void *ptr))
{
	EVP_CIPHER *cipher = EVP_CIPHER_meth_new(nid, block_size, key_len);

	if (cipher == NULL
		|| !EVP_CIPHER_meth_set_iv_length(cipher, iv_len)
		|| !EVP_CIPHER_meth_set_flags(cipher, flags)
		|| !EVP_CIPHER_meth_set_init(cipher, rts_cipher_init)
		|| !EVP_CIPHER_meth_set_do_cipher(cipher, rts_do_cipher)
		|| !EVP_CIPHER_meth_set_cleanup(cipher, rts_cipher_cleanup)
		|| !EVP_CIPHER_meth_set_impl_ctx_size(
			cipher, sizeof(struct rts_cipher_data))
		|| !EVP_CIPHER_meth_set_set_asn1_params(
			cipher, EVP_CIPHER_set_asn1_iv)
		|| !EVP_CIPHER_meth_set_get_asn1_params(
			cipher, EVP_CIPHER_get_asn1_iv)
		|| !EVP_CIPHER_meth_set_ctrl(cipher, ctrl)) {

		EVP_CIPHER_meth_free(cipher);
		return NULL;
	}

	return cipher;
}

static EVP_CIPHER *cipher_new_by_nid(int nid)
{
	switch (nid) {
	case NID_aes_128_ecb:
		return cipher_new(nid, AES_BLOCK_SIZE, AES_ECB_IVLEN,
				AES_KEY_SIZE_128, AES_ECB_FLAGS,
				NULL);
	case NID_aes_128_cbc:
		return cipher_new(nid, AES_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_128, AES_CBC_FLAGS,
				NULL);
	case NID_aes_128_ctr:
		return cipher_new(nid, AES_CTR_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_128, AES_CTR_FLAGS,
				NULL);
	case NID_aes_128_ccm:
		return cipher_new(nid, AES_CCM_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_128, AES_CCM_FLAGS,
				rts_aes_ccm_ctrl);
	case NID_aes_128_gcm:
		return cipher_new(nid, AES_GCM_BLOCK_SIZE, AES_GCM_IVLEN,
				AES_KEY_SIZE_128, AES_GCM_FLAGS,
				rts_aes_gcm_ctrl);
	case NID_aes_192_ecb:
		return cipher_new(nid, AES_BLOCK_SIZE, AES_ECB_IVLEN,
				AES_KEY_SIZE_192, AES_ECB_FLAGS,
				NULL);
	case NID_aes_192_cbc:
		return cipher_new(nid, AES_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_192, AES_CBC_FLAGS,
				NULL);
	case NID_aes_192_ctr:
		return cipher_new(nid, AES_CTR_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_192, AES_CTR_FLAGS,
				NULL);
	case NID_aes_192_ccm:
		return cipher_new(nid, AES_CCM_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_192, AES_CCM_FLAGS,
				rts_aes_ccm_ctrl);
	case NID_aes_192_gcm:
		return cipher_new(nid, AES_GCM_BLOCK_SIZE, AES_GCM_IVLEN,
				AES_KEY_SIZE_192, AES_GCM_FLAGS,
				rts_aes_gcm_ctrl);
	case NID_aes_256_ecb:
		return cipher_new(nid, AES_BLOCK_SIZE, AES_ECB_IVLEN,
				AES_KEY_SIZE_256, AES_ECB_FLAGS,
				NULL);
	case NID_aes_256_cbc:
		return cipher_new(nid, AES_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_256, AES_CBC_FLAGS,
				NULL);
	case NID_aes_256_ctr:
		return cipher_new(nid, AES_CTR_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_256, AES_CTR_FLAGS,
				NULL);
	case NID_aes_256_ccm:
		return cipher_new(nid, AES_CCM_BLOCK_SIZE, AES_BLOCK_SIZE,
				AES_KEY_SIZE_256, AES_CCM_FLAGS,
				rts_aes_ccm_ctrl);
	case NID_aes_256_gcm:
		return cipher_new(nid, AES_GCM_BLOCK_SIZE, AES_GCM_IVLEN,
				AES_KEY_SIZE_256, AES_GCM_FLAGS,
				rts_aes_gcm_ctrl);
	case NID_des_ecb:
	case NID_des_ede_ecb:
		return cipher_new(nid, DES_BLOCK_SIZE, AES_ECB_IVLEN,
				DES_KEY_SIZE, AES_ECB_FLAGS,
				NULL);
	case NID_des_cbc:
	case NID_des_ede_cbc:
		return cipher_new(nid, DES_BLOCK_SIZE, DES_BLOCK_SIZE,
				DES_KEY_SIZE, AES_CBC_FLAGS,
				NULL);
	case NID_des_ede3_ecb:
		return cipher_new(nid, DES_EDE3_BLOCK_SIZE, AES_ECB_IVLEN,
				DES_EDE3_KEY_SIZE, AES_ECB_FLAGS,
				NULL);
	case NID_des_ede3_cbc:
		return cipher_new(nid, DES_EDE3_BLOCK_SIZE, DES_EDE3_BLOCK_SIZE,
				DES_EDE3_KEY_SIZE, AES_CBC_FLAGS,
				NULL);
	default:
		break;
	}

	return NULL;
}

/* digest */
static int rts_digest_copy(EVP_MD_CTX *to, const EVP_MD_CTX *from)
{
	struct rts_digest_data *tddata, *fddata;

	LOG_INFO("ctx to=0x%08x, ctx from=0x%08x\n", to, from);
	if (!to || !from) {
		ENGINEerr(ENGINE_F_RTS_DIGEST_COPY,
				ERR_R_PASSED_NULL_PARAMETER);
		LOG_ERR("to or from ctx is null\n");
		return 0;
	}

	tddata = EVP_MD_CTX_md_data(to);
	fddata = EVP_MD_CTX_md_data(from);
	if (!tddata || !fddata) {
		LOG_WARN("tddata or fddata is null\n");
		return 1;
	}

	if (!fddata->tfm) {
		LOG_WARN("fddata tfm is null\n");
		return 1;
	}

	/* copy digest state by accept */
	if (fddata->tfm->init) {
		/* accept */
		tddata->io_fd = rts_accept(fddata->io_fd);
		if (tddata->io_fd < 0) {
			LOG_ERR("accept fail\n");
			ENGINEerr(ENGINE_F_RTS_DIGEST_COPY,
					ERR_R_OPERATION_FAIL);
			return 0;
		}
	}

	/* reference count++ */
	if (tddata->tfm == fddata->tfm)
		tddata->tfm->refcnt++;

	return 1;
}

static int rts_digest_cleanup(EVP_MD_CTX *ctx)
{
	struct rts_digest_data *ddata;

	LOG_INFO("ctx=0x%08x\n", ctx);

	ddata = EVP_MD_CTX_md_data(ctx);
	if (!ddata || !ddata->tfm) {
		LOG_WARN("ddata or tfm is null\n");
		return 1;
	}

	/* unaccept */
	if (ddata->tfm->init)
		rts_unaccept(&ddata->io_fd);

	/* reference count-- */
	if (ddata->tfm->refcnt) {
		ddata->tfm->refcnt--;
		return 1;
	}

	/* release tfm */
	release_tfm(ddata->tfm);
	free(ddata->tfm);
	ddata->tfm = NULL;

	return 1;
}

static int rts_digest_init(EVP_MD_CTX *ctx)
{
	struct rts_digest_data *ddata;
	struct rts_tfm *tfm;
	int ret, nid;

	ddata = EVP_MD_CTX_md_data(ctx);
	nid = EVP_MD_CTX_type(ctx);

	LOG_INFO("nid=%d, ctx=0x%08x\n", nid, ctx);

	if (!ddata->tfm) {
		ddata->tfm = calloc(1, sizeof(struct rts_tfm));
		if (!ddata->tfm) {
			LOG_ERR("calloc tfm  memory fail\n");
			ENGINEerr(ENGINE_F_RTS_DIGEST_INIT,
					ERR_R_MALLOC_FAILURE);
			return 0;
		}
	}
	tfm = ddata->tfm;

	if (tfm->init) {
		LOG_WARN("already initialized!\n");
		return 1;
	}

	ret = new_tfm(tfm, nid, -1);
	if (!ret)
		goto err;

	ddata->io_fd = rts_accept(tfm->sock_fd);
	if (ddata->io_fd < 0)
		goto err;

	return 1;
err:
	rts_digest_cleanup(ctx);
	ENGINEerr(ENGINE_F_RTS_DIGEST_INIT, ERR_R_INIT_FAIL);
	return 0;
}

static int rts_digest_update(EVP_MD_CTX *ctx,
			const void *data, size_t count)
{
	int ret;
	struct rts_digest_data *ddata;

	LOG_INFO("ctx=0x%08x, count=%d\n", ctx, count);

	ddata = EVP_MD_CTX_md_data(ctx);

	ret = send_data_buf(ddata->io_fd, NULL, data, count, MSG_MORE);
	if (!ret) {
		ENGINEerr(ENGINE_F_RTS_DIGEST_UPDATE,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	return 1;
}

static int rts_digest_final(EVP_MD_CTX *ctx, unsigned char *md)
{
	struct rts_digest_data *ddata;
	int result_size, ret;

	LOG_INFO("ctx=0x%08x\n", ctx);

	ddata = EVP_MD_CTX_md_data(ctx);
	result_size = EVP_MD_meth_get_result_size(EVP_MD_CTX_md(ctx));

	ret = send_data_buf(ddata->io_fd, md, NULL, result_size, 0);
	if (!ret) {
		ENGINEerr(ENGINE_F_RTS_DIGEST_FINAL,
					ERR_R_OPERATION_FAIL);
		return 0;
	}

	return 1;
}

static EVP_MD *digest_new(int nid, int block_size, int result_size,
				unsigned long flags)
{
	EVP_MD *digest = EVP_MD_meth_new(nid, 0);

	if (!digest
		|| !EVP_MD_meth_set_input_blocksize(digest, block_size)
		|| !EVP_MD_meth_set_result_size(digest, result_size)
		|| !EVP_MD_meth_set_app_datasize(digest,
			sizeof(struct rts_digest_data))
		|| !EVP_MD_meth_set_flags(digest, flags)
		|| !EVP_MD_meth_set_init(digest, rts_digest_init)
		|| !EVP_MD_meth_set_update(digest, rts_digest_update)
		|| !EVP_MD_meth_set_final(digest, rts_digest_final)
		|| !EVP_MD_meth_set_cleanup(digest, rts_digest_cleanup)
		|| !EVP_MD_meth_set_copy(digest, rts_digest_copy)
		) {

		EVP_MD_meth_free(digest);
		return NULL;
	}

	return digest;
}

static EVP_MD *digest_new_by_nid(int nid)
{
	switch (nid) {
	case NID_sha256:
		return digest_new(nid, SHA256_CBLOCK,
					SHA256_DIGEST_LENGTH, 0);
	default:
		break;
	}

	return NULL;
}

/* rsa */
struct rts_rsa_data rts_rdata;

static int rsa_cleanup(void)
{
	struct rts_rsa_data *rdata = &rts_rdata;

	LOG_INFO("\n");
	rts_unaccept(&rdata->io_fd);
	release_tfm(&rdata->tfm);

	if (rdata->key) {
		free(rdata->key);
		rdata->key = NULL;
	}

	return 1;
}

static int rsa_init(void)
{
	struct rts_rsa_data *rdata = &rts_rdata;
	int ret;

	LOG_INFO("\n");
	if (!rdata->key) {
		rdata->key = malloc(RSA_KEY_MAX_BYTES);
		if (!rdata->key) {
			LOG_ERR("malloc key memory fail\n");
			ENGINEerr(ENGINE_F_RSA_INIT,
						ERR_R_MALLOC_FAILURE);
			return -1;
		}
	}

	ret = new_tfm(&rdata->tfm, NID_rsa, -1);
	if (!ret) {
		ENGINEerr(ENGINE_F_RSA_INIT, ERR_R_INIT_FAIL);
		return -1;
	}

	rdata->io_fd = rts_accept(rdata->tfm.sock_fd);
	if (rdata->io_fd < 0) {
		ENGINEerr(ENGINE_F_RSA_INIT, ERR_R_INIT_FAIL);
		return -1;
	}

	return 1;
}

static int is_supported_rsa_keysize(int len)
{
	switch (len << 3) {
	case 1024:
	case 2048:
	case 3072:
		return 1;
	}

	LOG_DBG("not support keysize: %dB\n\n", len);
	return 0;
}

static int rsa_crypt(int flen, const unsigned char *from,
			unsigned char *to, RSA *rsa, int padding, int op)
{
	int ret, key_len, pubkey;
	struct rts_rsa_data *rdata = &rts_rdata;
	unsigned char *key;

	LOG_INFO("flen=%d, rsa=%x, padding=%d, op=%d\n",
				flen, rsa, padding, op);

	ret = rsa_init();
	if (!ret)
		goto err;

	/* MUST! i2d_RSA* api will modify the address of the key */
	key = rdata->key;
	if (!key)
		goto err;

	switch (op) {
	case ALG_OP_ENCRYPT:
		pubkey = 1;

		/* struct rsa convert to DER format key */
		key_len = i2d_RSAPublicKey(rsa, &key);
		if (key_len <= 0) {
			LOG_ERR("i2d_RSAPublicKey fail\n");
			ENGINEerr(ENGINE_F_RSA_CRYPT,
				ENGINE_R_FAILED_LOADING_PUBLIC_KEY);
			goto err;
		}
		break;
	case ALG_OP_DECRYPT:
		pubkey = 0;

		/* struct rsa convert to DER format key */
		key_len = i2d_RSAPrivateKey(rsa, &key);
		if (key_len <= 0) {
			LOG_ERR("i2d_RSAPrivateKey fail\n");
			ENGINEerr(ENGINE_F_RSA_CRYPT,
				ENGINE_R_FAILED_LOADING_PRIVATE_KEY);
			goto err;
		}
		break;
	default:
		goto err;
	}

	ret = set_key(rdata->tfm.sock_fd, rdata->key, key_len, pubkey);
	if (!ret)
		goto err;

	/* set control */
	ret = set_control(rdata->io_fd, op, 0, NULL, 0);
	if (!ret)
		goto err;

	ret = send_data_buf(rdata->io_fd, to, from, flen, 0);
	if (!ret)
		goto err;

	rsa_cleanup();
	return flen;
err:
	LOG_ERR("rsa crypt fail, ret=%d\n", ret);
	ENGINEerr(ENGINE_F_RSA_CRYPT, ERR_R_OPERATION_FAIL);
	rsa_cleanup();
	return -1;
}


/* encrypt */
static int rts_rsa_public_encrypt(int flen, const unsigned char *from,
			unsigned char *to, RSA *rsa, int padding)
{
	int ret = -1, num = 0;
	unsigned char *buf = NULL;

	LOG_INFO("\n");

	num = RSA_size(rsa);
	LOG_DBG("num = %d\n", num);

	if (!is_supported_rsa_keysize(num)) {
		LOG_WARN("use openssl rsa default method\n");
		ret = RSA_set_method(rsa, RSA_get_default_method());
		if (ret == -1)
			goto err;

		return RSA_public_encrypt(flen, from, to, rsa, padding);
	}

	buf = OPENSSL_malloc(num);
	if (!buf) {
		LOG_ERR("openssl malloc fail\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PUBLIC_ENCRYPT,
					ERR_R_MALLOC_FAILURE);
		goto err;
	}

	switch (padding) {
	case RSA_PKCS1_PADDING:
		ret = RSA_padding_add_PKCS1_type_2(buf, num, from, flen);
		break;
	case RSA_PKCS1_OAEP_PADDING:
		ret = RSA_padding_add_PKCS1_OAEP(buf, num, from, flen, NULL, 0);
		break;
	case RSA_NO_PADDING:
		ret = RSA_padding_add_none(buf, num, from, flen);
		break;
	default:
		LOG_ERR("unknown padding mode\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PUBLIC_ENCRYPT,
					ENGINE_R_UNKNOWN_PADDING_MODE);
		goto err;
	}

	if (ret <= 0) {
		LOG_ERR("padding failed, ret=%d\n", ret);
		goto err;
	}

	ret = rsa_crypt(num, buf, to, rsa, padding, ALG_OP_ENCRYPT);

	OPENSSL_clear_free(buf, num);
	return ret;
err:
	OPENSSL_clear_free(buf, num);
	ENGINEerr(ENGINE_F_RTS_RSA_PUBLIC_ENCRYPT, ERR_R_OPERATION_FAIL);
	return -1;
}

/* decrypt */
static int rts_rsa_private_decrypt(int flen, const unsigned char *from,
			unsigned char *to, RSA *rsa, int padding)
{
	int ret = -1, num = 0;
	unsigned char *buf = NULL;

	LOG_INFO("\n");

	num = RSA_size(rsa);
	LOG_DBG("num = %d\n", num);

	if (!is_supported_rsa_keysize(num)) {
		LOG_WARN("use openssl rsa default method\n");
		ret = RSA_set_method(rsa, RSA_get_default_method());
		if (ret == -1)
			goto err;

		return RSA_private_decrypt(flen, from, to, rsa, padding);
	}

	buf = OPENSSL_malloc(num);
	if (!buf) {
		LOG_ERR("openssl malloc fail\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PRIVATE_DECRYPT,
					ERR_R_MALLOC_FAILURE);
		goto err;
	}

	ret = rsa_crypt(flen, from, buf, rsa, padding, ALG_OP_DECRYPT);
	if (ret == -1)
		goto err;

	switch (padding) {
	case RSA_PKCS1_PADDING:
		ret = RSA_padding_check_PKCS1_type_2(to, num, buf, ret, num);
	break;
	case RSA_PKCS1_OAEP_PADDING:
		ret = RSA_padding_check_PKCS1_OAEP(to, num, buf,
					ret, num, NULL, 0);
		break;
	case RSA_NO_PADDING:
		memcpy(to, buf, ret);
		break;
	default:
		LOG_ERR("unknown padding mode\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PRIVATE_DECRYPT,
					ENGINE_R_UNKNOWN_PADDING_MODE);
		goto err;
	}

	if (ret <= 0) {
		LOG_ERR("padding failed, ret=%d\n", ret);
		goto err;
	}

	OPENSSL_clear_free(buf, num);
	return ret;
err:
	OPENSSL_clear_free(buf, num);
	ENGINEerr(ENGINE_F_RTS_RSA_PRIVATE_DECRYPT, ERR_R_OPERATION_FAIL);
	return -1;
}

/* sign */
static int rts_rsa_private_encrypt(int flen, const unsigned char *from,
			unsigned char *to, RSA *rsa, int padding)
{
	int ret = -1, num = 0;
	unsigned char *buf = NULL;

	LOG_INFO("\n");

	num = RSA_size(rsa);
	LOG_DBG("num = %d\n", num);

	if (!is_supported_rsa_keysize(num)) {
		LOG_WARN("use openssl rsa default method\n");
		ret = RSA_set_method(rsa, RSA_get_default_method());
		if (ret == -1)
			goto err;

		return RSA_private_encrypt(flen, from, to, rsa, padding);
	}

	buf = OPENSSL_malloc(num);
	if (!buf) {
		LOG_ERR("openssl malloc fail\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PRIVATE_ENCRYPT,
					ERR_R_MALLOC_FAILURE);
		goto err;
	}

	switch (padding) {
	case RSA_PKCS1_PADDING:
		ret = RSA_padding_add_PKCS1_type_1(buf, num, from, flen);
		break;
	case RSA_X931_PADDING:
		ret = RSA_padding_add_X931(buf, num, from, flen);
		break;
	case RSA_NO_PADDING:
		ret = RSA_padding_add_none(buf, num, from, flen);
		break;
	default:
		LOG_ERR("unknown padding mode\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PRIVATE_ENCRYPT,
					ENGINE_R_UNKNOWN_PADDING_MODE);
		goto err;
	}

	if (ret <= 0) {
		LOG_ERR("padding failed, ret=%d\n", ret);
		goto err;
	}

	ret = rsa_crypt(num, buf, to, rsa, padding, ALG_OP_DECRYPT);

	OPENSSL_clear_free(buf, num);
	return ret;
err:
	OPENSSL_clear_free(buf, num);
	ENGINEerr(ENGINE_F_RTS_RSA_PRIVATE_ENCRYPT, ERR_R_OPERATION_FAIL);
	return -1;
}

/* verify */
static int rts_rsa_public_decrypt(int flen, const unsigned char *from,
			unsigned char *to, RSA *rsa, int padding)
{
	int ret = -1, num = 0;
	unsigned char *buf = NULL;

	LOG_INFO("\n");

	num = RSA_size(rsa);
	LOG_DBG("num = %d\n", num);

	if (!is_supported_rsa_keysize(num)) {
		LOG_WARN("use openssl rsa default method\n");
		ret = RSA_set_method(rsa, RSA_get_default_method());
		if (ret == -1)
			goto err;

		return RSA_public_decrypt(flen, from, to, rsa, padding);
	}

	buf = OPENSSL_malloc(num);
	if (!buf) {
		LOG_ERR("openssl malloc fail\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PUBLIC_DECRYPT,
					ERR_R_MALLOC_FAILURE);
		goto err;
	}

	ret = rsa_crypt(flen, from, buf, rsa, padding, ALG_OP_ENCRYPT);
	if (ret == -1)
		goto err;

	switch (padding) {
	case RSA_PKCS1_PADDING:
		ret = RSA_padding_check_PKCS1_type_1(to, num, buf, ret, num);
		break;
	case RSA_X931_PADDING:
		ret = RSA_padding_check_X931(to, num, buf, ret, num);
		break;
	case RSA_NO_PADDING:
		memcpy(to, buf, ret);
		break;
	default:
		LOG_ERR("unknown padding mode\n");
		ENGINEerr(ENGINE_F_RTS_RSA_PUBLIC_DECRYPT,
					ENGINE_R_UNKNOWN_PADDING_MODE);
		goto err;
	}

	if (ret <= 0) {
		LOG_ERR("padding failed, ret=%d\n", ret);
		goto err;
	}

	OPENSSL_clear_free(buf, num);
	return ret;
err:
	OPENSSL_clear_free(buf, num);
	ENGINEerr(ENGINE_F_RTS_RSA_PUBLIC_DECRYPT, ERR_R_OPERATION_FAIL);
	return -1;
}

int rsa_method_init(void)
{
	if (!rts_rsa_method
		|| !RSA_meth_set_pub_enc(rts_rsa_method,
			rts_rsa_public_encrypt)
		|| !RSA_meth_set_priv_dec(rts_rsa_method,
			rts_rsa_private_decrypt)
		|| !RSA_meth_set_priv_enc(rts_rsa_method,
			rts_rsa_private_encrypt)
		|| !RSA_meth_set_pub_dec(rts_rsa_method,
			rts_rsa_public_decrypt)
		)
		return 0;

	return 1;
}

/* engine */
static int rts_init(ENGINE *e)
{
	int i;

	/* init cipher */
	for (i = 0; i < rts_cipher_nids_num; i++)
		rciphers[i] = cipher_new_by_nid(rts_cipher_nids[i]);

	/* init digest */
	for (i = 0; i < rts_digest_nids_num; i++)
		rdigests[i] = digest_new_by_nid(rts_digest_nids[i]);

	/* init rsa */
	return rsa_method_init();
}

static int rts_destroy(ENGINE *e)
{
	int i;

	/* destroy cipher */
	for (i = 0; i < rts_cipher_nids_num; i++)
		EVP_CIPHER_meth_free(rciphers[i]);

	/* destroy digest */
	for (i = 0; i < rts_digest_nids_num; i++)
		EVP_MD_meth_free(rdigests[i]);

	/* destroy rsa */
	if (rts_rsa_method)
		RSA_meth_free(rts_rsa_method);

	return 1;
}

static int rts_ciphers(ENGINE *e, const EVP_CIPHER **cipher,
			const int **nids, int nid)
{
	int i;

	if (!cipher) {
		*nids = rts_cipher_nids;
		return rts_cipher_nids_num;
	}

	for (i = 0; i < rts_cipher_nids_num; i++) {
		if (rts_cipher_nids[i] == nid) {
			*cipher = rciphers[i];
			return 1;
		}
	}

	*cipher = NULL;
	return 0;
}

static int rts_digests(ENGINE *e, const EVP_MD **digest,
			const int **nids, int nid)
{
	int i;

	if (!digest) {
		*nids = rts_digest_nids;
		return rts_digest_nids_num;
	}

	for (i = 0; i < rts_digest_nids_num; i++) {
		if (rts_digest_nids[i] == nid) {
			*digest = rdigests[i];
			return 1;
		}
	}

	*digest = NULL;
	return 0;
}

static int get_random_bytes(unsigned char *buf, int num)
{
	int fd = -1, n;

	if (num < 0 || !buf)
		return 0;

	fd = open("/dev/hwrng", O_RDONLY);
	if (fd < 0)
		return 0;
	n = read(fd, buf, num);
	if (n < 0)
		n = 0;
	close(fd);
	return n;
}

static int random_status(void)
{
	return 1;
}

static RAND_METHOD rts_rand_meth = {
	NULL,
	get_random_bytes,
	NULL,
	NULL,
	get_random_bytes,
	random_status,
};

static int bind_helper(ENGINE *e)
{
	rts_rsa_method = RSA_meth_new("RTS RSA Method", 0);
	if (!rts_rsa_method)
		return 0;

	if (!ENGINE_set_id(e, engine_e_rts_id) ||
		!ENGINE_set_name(e, engine_e_rts_name) ||
		!ENGINE_set_init_function(e, rts_init) ||
		!ENGINE_set_destroy_function(e, rts_destroy) ||
		!ENGINE_set_ciphers(e, rts_ciphers) ||
		!ENGINE_set_digests(e, rts_digests) ||
		!ENGINE_set_RSA(e, rts_rsa_method) ||
		!ENGINE_set_RAND(e, &rts_rand_meth))
		return 0;

	return 1;
}

static ENGINE *ENGINE_rts(void)
{
	ENGINE *ret = ENGINE_new();

	if (!ret)
		return NULL;

	if (!bind_helper(ret)) {
		ENGINE_free(ret);
		return NULL;
	}

	return ret;
}

void ENGINE_load_rts(void)
{
	ENGINE *toadd = ENGINE_rts();

	if (!toadd)
		return;

	ENGINE_add(toadd);
	ENGINE_free(toadd);
	ERR_clear_error();
}

#else

void ENGINE_load_rts(void)
{

}

#endif
