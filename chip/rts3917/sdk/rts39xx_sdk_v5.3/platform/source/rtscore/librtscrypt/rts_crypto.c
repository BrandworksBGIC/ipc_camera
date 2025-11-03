#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/socket.h>
#include <linux/if_alg.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "rts_crypto_log.h"
#include "cryptouser.h"
#include "rts_crypto.h"


#ifndef SOL_ALG
#define SOL_ALG			279
#endif

#define ENCRYPT_UNIT_LEN_MAX	(160 * 1024) /* byte */
#define PKCS7_MAX_BYTE		255
#define CIPHER_ALIGNMENT	32 /* byte */

#define RTS_CIPHER_IOC_MAGIC	0x81
#define RTS_CIPHER_IOC_SET_NORMALKEY	_IO(RTS_CIPHER_IOC_MAGIC, 0xA0)
#define RTS_CIPHER_IOC_SET_EFUSEKEY	_IO(RTS_CIPHER_IOC_MAGIC, 0xA1)

enum RTS_CRYPTO_LOG_LEVEL crypto_log_level = CRYPTO_LOG_NONE;

enum rts_cipher_type {
	RTS_CIPHER_SKCIPHER,
};

struct rts_cipher_info {
	enum rts_cipher_type cipher_type;
	/* generic */
	uint32_t blocksize;
	uint32_t ivsize;
	/* blkcipher */
	uint32_t blk_min_keysize;
	uint32_t blk_max_keysize;
};

struct cipher_padding {
	uint32_t len;		/* padding data len */
	uint8_t *buf;		/* CIPHER_ALIGNMENT */
	uint32_t left_len;	/* left data len */
};

struct rts_tfm {
	char name[128];
	int tfmfd;
	struct rts_cipher_info cipher_info;
};

struct rts_cipher_handle {
	struct rts_tfm *tfm;
	int opfd;
	uint8_t *iv;
	enum RTS_CIPHER_PADDING_MODE padding_mode;
	struct cipher_padding padding;
	uint8_t *align_buf;
};

struct crypto_nlmsg {
	struct nlmsghdr msgh;
	struct crypto_user_alg ualg;
};

#define check_cipher_init(handle) \
		(handle && handle->tfm && handle->opfd > 0)

#define check_addr_alignment(addr, alignment) \
		(!((uint32_t)(addr) & ((alignment) - 1)))

#define align_addr_offset(addr, alignment) \
		((alignment) - ((uint32_t)(addr) % (alignment)))

#define align_byte(b, alignment) \
		(((b) / (alignment) + 1) * (alignment))

#define cipher_aligned_alloc(size, alignment) \
		(aligned_alloc((alignment), align_byte((size), (alignment))))

static uint8_t g_key[16];
static int g_keymode;

static int cipher_padding_init(struct cipher_padding *padding,
			uint32_t blocksize)
{
	if (!padding)
		return -RTS_E_CIPHER_NULL_POINT;

	if (blocksize > PKCS7_MAX_BYTE) {
		rts_crypto_error(
			"pkcs#7: blocksize is greater than 255 byte\n");
		return -RTS_E_CIPHER_NOT_SUPPORT;
	}

	padding->len = blocksize;
	padding->left_len = 0;

	if (!padding->buf) {
		padding->buf = (uint8_t *)cipher_aligned_alloc(
					blocksize, 2 * CIPHER_ALIGNMENT);
		if (!padding->buf) {
			rts_crypto_error("out of memory\n");
			return -RTS_E_CIPHER_NO_MEMORY;
		}
	}

	return 0;
}

static void cipher_padding_destroy(struct cipher_padding *padding)
{
	if (padding && padding->buf) {
		free(padding->buf);
		padding->buf = NULL;
	}

	padding->len = 0;
	padding->left_len = 0;
}

static int cipher_padding(struct cipher_padding *padding,
			const uint8_t *left_in, uint32_t left_len)
{
	int i;
	uint8_t value;

	rts_crypto_info("left_len = %d\n", left_len);

	if (!padding)
		return -RTS_E_CIPHER_NULL_POINT;

	if (left_len && !left_in)
		return -RTS_E_CIPHER_INVALID_ARG;

	if (left_len < 0 || left_len > padding->len)
		return -RTS_E_CIPHER_INVALID_ARG;

	padding->left_len = left_len;
	value = padding->len - left_len;
	rts_crypto_info("padding value = %d\n", value);

	/* left data*/
	if (left_len && left_in)
		memcpy(padding->buf, left_in, left_len);

	/* padding data*/
	for (i = left_len; i < padding->len; i++)
		padding->buf[i] = value;

	return 0;
}

static int cipher_remove_padding(struct cipher_padding *padding,
			uint8_t *in, uint32_t *len)
{
	int i;
	uint8_t value;

	rts_crypto_info("len = %d\n", *len);

	if (!padding)
		return -RTS_E_CIPHER_NULL_POINT;

	if (!in || !len)
		return -RTS_E_CIPHER_INVALID_ARG;

	value = in[*len - 1];
	rts_crypto_info("padding value = %d\n", value);

	if (value == 0 || value  > padding->len)
		return -RTS_FAIL;

	for (i = 1; i <= value; i++)
		in[*len - i] = 0;

	*len -= value;

	return 0;
}

static int is_need_padding(struct rts_cipher_handle *handle)
{
	if (!handle && !handle->tfm)
		return 0;

	if (handle->tfm->cipher_info.blocksize == 1)
		return 0;

	if (strcasestr(handle->tfm->name, "ctr"))
		return 0;

	return 1;
}

static int __rts_get_cipher_info(struct rts_cipher_handle *handle,
			const char *name)
{
	int ret;
	struct sockaddr_nl nl;
	int sd;
	struct msghdr msg;
	struct crypto_nlmsg cnlmsg;
	struct iovec iov;
	char buf[4096];
	struct nlmsghdr *recv_msgh = (struct nlmsghdr *)buf;
	int recv_len = 0;
	struct crypto_user_alg *recv_ualg = NULL;
	struct rtattr *rta = NULL;
	struct crypto_report_blkcipher *recv_blk = NULL;

	if (!handle || !name) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	/* netlink socket */
	sd = socket(AF_NETLINK, SOCK_RAW, NETLINK_CRYPTO);
	if (sd < 0) {
		rts_crypto_error("socket fail, errno = %d\n", errno);
		return -RTS_FAIL;
	}

	/* bind */
	memset(&nl, 0, sizeof(nl));
	nl.nl_family = AF_NETLINK;

	ret = bind(sd, (struct sockaddr *)&nl, sizeof(nl));
	if (ret < 0) {
		rts_crypto_error("bind fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
		goto err;
	}


	/* init msghdr*/
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &nl;
	msg.msg_namelen = sizeof(nl);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* send data*/
	/* init crypto_nlmsg*/
	memset(&cnlmsg, 0, sizeof(cnlmsg));
	cnlmsg.msgh.nlmsg_len = NLMSG_LENGTH(sizeof(cnlmsg.ualg));
	cnlmsg.msgh.nlmsg_flags = NLM_F_REQUEST;
	cnlmsg.msgh.nlmsg_type = CRYPTO_MSG_GETALG;
	cnlmsg.msgh.nlmsg_seq = time(NULL);
	strncpy(cnlmsg.ualg.cru_name, name, sizeof(cnlmsg.ualg.cru_name));
	rts_crypto_info("name = %s, cru_name = %s, driver name = %s\n",
			name, cnlmsg.ualg.cru_name,
			cnlmsg.ualg.cru_driver_name);

	iov.iov_base = (void *)&cnlmsg.msgh;
	iov.iov_len = cnlmsg.msgh.nlmsg_len;

	ret = sendmsg(sd, &msg, 0);
	if (ret < 0) {
		rts_crypto_error("sendmsg fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
		goto err;
	}

	/* receive data*/
	memset(buf, 0, sizeof(buf));
	iov.iov_base = (void *)buf;
	iov.iov_len = sizeof(buf);

	while (1) {
		ret = recvmsg(sd, &msg, 0);
		if (ret < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;

			rts_crypto_error("recvmsg fail, errno = %d\n", errno);
			ret = -RTS_FAIL;
			goto err;
		}
		if (ret == 0) {
			rts_crypto_error("recvmsg fail: no data\n");
			ret = -RTS_FAIL;
			goto err;
		}
		if (ret > sizeof(buf)) {
			rts_crypto_error("recvmsg fail: too much data\n");
			ret = -RTS_FAIL;
			goto err;
		}
		break;
	}

	recv_len = recv_msgh->nlmsg_len;
	rts_crypto_info("recv_len = %d, nlmsg ytpe: %d\n",
				recv_len, recv_msgh->nlmsg_type);
	if (recv_msgh->nlmsg_type == NLMSG_ERROR) {
		rts_crypto_error("recv msgh nlmsg_type = NLMSG_ERROR\n");
		ret = -RTS_FAIL;
		goto err;
	}

	if (recv_msgh->nlmsg_type == CRYPTO_MSG_GETALG) {
		recv_ualg = NLMSG_DATA(recv_msgh);
		rts_crypto_info(
				"name = %s, driver_name = %s, modulue_name = %s\n",
				recv_ualg->cru_name,
				recv_ualg->cru_driver_name,
				recv_ualg->cru_module_name);
		recv_len -= NLMSG_SPACE(sizeof(struct crypto_user_alg));
	}

	rts_crypto_info("recv_len = %d\n", recv_len);
	if (recv_len < 0) {
		rts_crypto_error("recv_len error(<0)\n");
		ret = -RTS_FAIL;
		goto err;
	}

	/* parse data */
	rta = (struct rtattr *)((char *)recv_ualg +
			NLMSG_ALIGN(sizeof(struct crypto_user_alg)));

	while (RTA_OK(rta, recv_len)) {
		if (rta->rta_type == CRYPTOCFGA_REPORT_BLKCIPHER) {
			rts_crypto_info("get blkciper info\n");
			recv_blk = (struct crypto_report_blkcipher *)
				RTA_DATA(rta);
			handle->tfm->cipher_info.cipher_type =
				RTS_CIPHER_SKCIPHER;
			handle->tfm->cipher_info.blocksize =
				recv_blk->blocksize;
			handle->tfm->cipher_info.ivsize =
				recv_blk->ivsize;
			handle->tfm->cipher_info.blk_min_keysize =
				recv_blk->min_keysize;
			handle->tfm->cipher_info.blk_max_keysize =
				recv_blk->max_keysize;
			break;
		}

		rta = RTA_NEXT(rta, recv_len);
	}

	if (recv_len <= 0) {
		rts_crypto_error("not support cipher type = %d\n",
					rta->rta_type);
		ret = -RTS_E_CIPHER_NOT_SUPPORT;
		goto err;
	}

	ret = 0;
err:
	if (sd > 0)
		close(sd);
	return ret;
}

static void __rts_handle_destroy_tfm(struct rts_cipher_handle *handle)
{
	if (handle && handle->tfm) {
		if (handle->tfm->tfmfd != -1)
			close(handle->tfm->tfmfd);
		free(handle->tfm);
		handle->tfm = NULL;
	}
}

static int __rts_handle_init_tfm(struct rts_cipher_handle *handle,
			const char *type, const char *name)
{
	int ret;
	struct sockaddr_alg sa;

	if (!handle || !type || !name) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	if (!handle->tfm) {
		handle->tfm = calloc(1, sizeof(struct rts_tfm));
		if (!handle->tfm) {
			rts_crypto_error("out of memory\n");
			return -RTS_E_CIPHER_NO_MEMORY;
		}
	}

	strncpy(handle->tfm->name, name, sizeof(handle->tfm->name));

	memset(&sa, 0, sizeof(sa));
	sa.salg_family = AF_ALG;
	snprintf((char *)sa.salg_type, sizeof(sa.salg_type), "%s", type);
	snprintf((char *)sa.salg_name, sizeof(sa.salg_name), "%s", name);

	/* socket */
	handle->tfm->tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (handle->tfm->tfmfd < 0) {
		rts_crypto_error("socket fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
		goto err;
	}

	/* bind */
	ret = bind(handle->tfm->tfmfd, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0) {
		rts_crypto_error("bind fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
		goto err;
	}

	/* get cipher info */
	ret = __rts_get_cipher_info(handle, name);
	if (ret) {
		rts_crypto_error("get cipher info fail\n");
		goto err;
	}

	return 0;
err:
	__rts_handle_destroy_tfm(handle);
	return ret;
}

static void __rts_handle_destroy(struct rts_cipher_handle *handle)
{
	if (handle) {
		if (handle->opfd > 0) {
			close(handle->opfd);
			handle->opfd = 0;
		}

		if (handle->iv) {
			free(handle->iv);
			handle->iv = NULL;
		}

		cipher_padding_destroy(&handle->padding);

		if (handle->align_buf) {
			free(handle->align_buf);
			handle->align_buf = NULL;
		}

		__rts_handle_destroy_tfm(handle);
		free(handle);
		handle = NULL;
	}
}

static int __rts_handle_init(struct rts_cipher_handle **handle,
			const char *type, const char *name)
{
	int ret;

	if (!handle) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	*handle = calloc(1, sizeof(struct rts_cipher_handle));
	if (!*handle) {
		rts_crypto_error("out of memory\n");
		return -RTS_E_CIPHER_NO_MEMORY;
	}

	/* init trm */
	ret = __rts_handle_init_tfm(*handle, type, name);
	if (ret) {
		rts_crypto_error("init tfm fail\n");
		goto err;
	}

	/* init op */
	(*handle)->opfd = accept((*handle)->tfm->tfmfd, NULL, 0);
	if ((*handle)->opfd < 0) {
		rts_crypto_error("accept fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
		goto err;
	}

	/* init iv */
	if (!(*handle)->iv) {
		(*handle)->iv = (uint8_t *)calloc
			(1, (*handle)->tfm->cipher_info.ivsize);
		if (!(*handle)->iv) {
			rts_crypto_error("out of memory\n");
			ret = -RTS_E_CIPHER_NO_MEMORY;
			goto err;
		}
	}

	/* init padding */
	(*handle)->padding_mode = RTS_CIPHER_PADDING_NONE;
	if (is_need_padding(*handle)) {
		ret = cipher_padding_init(&(*handle)->padding,
				(*handle)->tfm->cipher_info.blocksize);
		if (ret) {
			rts_crypto_error(
				"cipher padding init fail, ret = %d\n",
				ret);
			goto err;
		}
	}

	/* init align buf*/
	(*handle)->align_buf = cipher_aligned_alloc(
				2 * CIPHER_ALIGNMENT, CIPHER_ALIGNMENT);
	if (!(*handle)->align_buf) {
		rts_crypto_error("out of memory\n");
		return -RTS_E_CIPHER_NO_MEMORY;
	}

	return 0;
err:
	__rts_handle_destroy(*handle);
	return ret;
}

static int __rts_setkey(struct rts_cipher_handle *handle,
			const uint8_t *key, uint32_t keysize)
{
	int ret;

	if (!check_cipher_init(handle)) {
		rts_crypto_error("not initialized\n");
		return -RTS_E_CIPHER_NOT_INITIALIZED;
	}

	if (!key) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	if (keysize < handle->tfm->cipher_info.blk_min_keysize ||
		keysize > handle->tfm->cipher_info.blk_max_keysize) {
		rts_crypto_error("key size is invalid\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	ret = setsockopt(handle->tfm->tfmfd, SOL_ALG, ALG_SET_KEY,
				key, keysize);
	if (ret < 0) {
		rts_crypto_error("setsockopt fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
	}

	return ret;
}

static int __rts_crypt(struct rts_cipher_handle *handle,
			const uint8_t *in, uint8_t *out, uint32_t len,
			int op_type, int set_iv)
{
	int ret, ivsize;
	struct msghdr in_msg, out_msg;
	struct iovec in_iov, out_iov;
	struct cmsghdr *cmsg;
	struct af_alg_iv *af_iv;
	const uint8_t *iv;

	ivsize = handle->tfm->cipher_info.ivsize;
	iv = handle->iv;

	if (ivsize && !iv) {
		rts_crypto_error("need set iv\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	/* send msg */
	/* init msghdr*/
	/* iovec store plaintext*/
	memset(&in_msg, 0, sizeof(in_msg));
	in_iov.iov_base = (void *)in;
	in_iov.iov_len = len;
	in_msg.msg_iov = &in_iov;
	in_msg.msg_iovlen = 1;

	/* msg_control stroe iv & op*/
	if (!ivsize || !set_iv)
		in_msg.msg_controllen = CMSG_SPACE(sizeof(int));
	else
		in_msg.msg_controllen = CMSG_SPACE(sizeof(int)) +
			CMSG_SPACE(sizeof(struct af_alg_iv) + ivsize);
	in_msg.msg_control = (void *)calloc(1, in_msg.msg_controllen);
	if (!in_msg.msg_control) {
		rts_crypto_error("out of memory\n");
		return -RTS_E_CIPHER_NO_MEMORY;
	}

	/* store op*/
	cmsg = CMSG_FIRSTHDR(&in_msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	memcpy(CMSG_DATA(cmsg), &op_type, sizeof(op_type));

	/* store iv */
	if (ivsize && set_iv) {
		cmsg = CMSG_NXTHDR(&in_msg, cmsg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct af_alg_iv) + ivsize);
		cmsg->cmsg_level = SOL_ALG;
		cmsg->cmsg_type = ALG_SET_IV;
		af_iv = (struct af_alg_iv *)CMSG_DATA(cmsg);
		af_iv->ivlen = ivsize;
		memcpy(af_iv->iv, iv, ivsize);
	}

	ret = sendmsg(handle->opfd, &in_msg, 0);
	if (ret < 0) {
		rts_crypto_error("sendmsg fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
		goto err;
	}

	/* recv msg */
	/* init msghdr*/
	/* iovec store ciphertext*/
	memset(&out_msg, 0, sizeof(out_msg));
	out_iov.iov_base = (void *)out;
	out_iov.iov_len = len;
	out_msg.msg_iov = &out_iov;
	out_msg.msg_iovlen = 1;

	ret = recvmsg(handle->opfd, &out_msg, 0);
	if (ret < 0) {
		rts_crypto_error("recvmsg fail, errno = %d\n", errno);
		ret = -RTS_FAIL;
		goto err;
	}

	ret = 0;
err:
	if (in_msg.msg_control) {
		free(in_msg.msg_control);
		in_msg.msg_control = NULL;
	}

	return ret;
}

static int __rts_crypt_spliter(struct rts_cipher_handle *handle,
			const uint8_t *in, uint32_t inlen,
			uint8_t *out, uint32_t *outlen,
			int op_type)
{
	int ret, set_iv;
	uint32_t len_offset, blocksize;
	uint8_t *in_aligned, *out_aligned, *out_tmp, *out_tmp2;
	const uint8_t *in_tmp, *in_tmp2;

	if (!handle || !in || !out || !outlen) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	blocksize = handle->tfm->cipher_info.blocksize;
	set_iv = -1;
	in_tmp = in;
	out_tmp = out;
	in_aligned = out_aligned = NULL;
	in_tmp2 = out_tmp2 = NULL;

	/* in buff alignment*/
	if (!check_addr_alignment(in, CIPHER_ALIGNMENT)) {
		rts_crypto_info("aligned alloc in buff\n");
		in_aligned = cipher_aligned_alloc(inlen, CIPHER_ALIGNMENT);
		if (!in_aligned) {
			rts_crypto_error("out of memory\n");
			return -RTS_E_CIPHER_NO_MEMORY;
		}
		memcpy(in_aligned, in, inlen);
		in_tmp = in_aligned;
	}

	/*  out buff alignment*/
	if (!check_addr_alignment(out, CIPHER_ALIGNMENT)) {
		rts_crypto_info("aligned alloc out buff\n");
		out_aligned = cipher_aligned_alloc(*outlen, CIPHER_ALIGNMENT);
		if (!out_aligned) {
			rts_crypto_error("out of memory\n");
			ret = -RTS_E_CIPHER_NO_MEMORY;
			goto err;
		}
		out_tmp = out_aligned;
	}

	/* calc ciphertext length*/
	*outlen = inlen;

	/* split packet*/
	while (inlen) {
		if (inlen >= ENCRYPT_UNIT_LEN_MAX) {
			len_offset = ENCRYPT_UNIT_LEN_MAX;
			rts_crypto_info("MAX, inlen = %d, len_offset = %d\n",
						inlen, len_offset);
		} else if (inlen >= CIPHER_ALIGNMENT) {
			len_offset = (inlen / blocksize) * blocksize;
			len_offset &= ~(CIPHER_ALIGNMENT - 1);
			rts_crypto_info("ALIGNMENT, inlen = %d, len_offset = %d\n",
						inlen, len_offset);
		} else if (inlen >= blocksize) {
			len_offset = (inlen / blocksize) * blocksize;
			memcpy(handle->align_buf, in_tmp, len_offset);
			in_tmp2 = in_tmp;
			in_tmp = handle->align_buf;
			out_tmp2 = out_tmp;
			out_tmp = handle->align_buf + CIPHER_ALIGNMENT;
			rts_crypto_info(
				"nedd align, inlen = %d, len_offset = %d\n",
				inlen, len_offset);
		} else {
			rts_crypto_info("need padding, inlen = %d\n",
						inlen);
			break;
		}

		inlen -= len_offset;
		set_iv = set_iv == -1 ? 1 : 0;

		ret = __rts_crypt(handle, in_tmp, out_tmp,
					len_offset, op_type, set_iv);
		if (ret) {
			rts_crypto_error("__rts_crypt failed\n");
			goto err;
		}

		if (in_tmp2) {
			in_tmp = in_tmp2;
			in_tmp2 = NULL;
		}

		if (out_tmp2) {
			memcpy(out_tmp2, out_tmp, len_offset);
			out_tmp = out_tmp2;
			out_tmp2 = NULL;
		}

		in_tmp += len_offset;
		out_tmp += len_offset;
	}

	/* padding: [0, blocksize) */
	if (op_type == ALG_OP_ENCRYPT &&
		handle->padding_mode != RTS_CIPHER_PADDING_NONE) {
		len_offset = inlen;
		in_tmp = inlen ? in_tmp : NULL;
		inlen = 0;

		ret = cipher_padding(&handle->padding, in_tmp, len_offset);
		if (ret) {
			rts_crypto_error("cipher padding failed\n");
			goto err;
		}

		len_offset = handle->padding.len;
		in_tmp = handle->padding.buf;
		out_tmp2 = out_tmp;
		out_tmp = handle->padding.buf + CIPHER_ALIGNMENT;
		/* calc ciphertext length*/
		*outlen += (handle->padding.len - handle->padding.left_len);

		/* encrypt */
		set_iv = set_iv == -1 ? 1 : 0;
		ret = __rts_crypt(handle, in_tmp, out_tmp,
					len_offset, ALG_OP_ENCRYPT, set_iv);
		if (ret) {
			rts_crypto_error("__rts_crypt failed\n");
			goto err;
		}

		memcpy(out_tmp2, out_tmp, len_offset);
	}

	if (out_aligned)
		memcpy(out, out_aligned, *outlen);

	/* remove padding */
	if (op_type == ALG_OP_DECRYPT &&
		handle->padding_mode != RTS_CIPHER_PADDING_NONE) {
		ret = cipher_remove_padding(&handle->padding, out, outlen);
		if (ret) {
			rts_crypto_error("cipher remove padding failed\n");
			goto err;
		}
	}

	rts_crypto_info("actual outlen = %d\n", *outlen);

err:
	if (in_aligned) {
		free(in_aligned);
		in_aligned = NULL;
	}

	if (out_aligned) {
		free(out_aligned);
		out_aligned = NULL;
	}

	return ret;
}

int rts_cipher_init(struct rts_cipher_handle **handle, const char *name)
{
	return	__rts_handle_init(handle, "skcipher", name);
}

void rts_cipher_destroy(struct rts_cipher_handle *handle)
{
	__rts_handle_destroy(handle);
}

int rts_cipher_set_keymode(enum RTS_CIPHER_KEYMODE mode)
{
	int ret, fd;

	fd = open("/dev/crypto", O_RDWR);
	if (fd < 0) {
		rts_crypto_error("open /dev/crypto failed\n");
		return -RTS_FAIL;
	}

	switch (mode) {
	case RTS_CIPHER_KEYMODE_NORMAL:
		ret = ioctl(fd, RTS_CIPHER_IOC_SET_NORMALKEY, NULL);
		if (ret < 0) {
			rts_crypto_error(
				"ioctl NORMALKEY failed, ret = %d\n", ret);
			ret = -RTS_FAIL;
		}
		break;
	case RTS_CIPHER_KEYMODE_EFUSE:
		ret = ioctl(fd, RTS_CIPHER_IOC_SET_EFUSEKEY, NULL);
		if (ret < 0) {
			rts_crypto_error(
				"ioctl EFUSEKEY failed, ret = %d\n", ret);
			ret = -RTS_FAIL;
		}
		break;
	default:
		rts_crypto_error("unsupported keymode\n");
		ret = -RTS_E_CIPHER_INVALID_ARG;
		break;
	}

	if (!ret)
		g_keymode = mode;

	if (fd)
		close(fd);

	return ret;
}

int rts_cipher_setkey(struct rts_cipher_handle *handle,
			const uint8_t *key, uint32_t keysize)
{
	return __rts_setkey(handle, key, keysize);
}

int rts_cipher_get_blocksize(struct rts_cipher_handle *handle,
			uint32_t *bksize)
{
	if (!check_cipher_init(handle)) {
		rts_crypto_error("not initialized\n");
		return -RTS_E_CIPHER_NOT_INITIALIZED;
	}

	if (!handle || !bksize) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	*bksize = handle->tfm->cipher_info.blocksize;

	return 0;
}

int rts_cipher_get_ivsize(struct rts_cipher_handle *handle, uint32_t *ivsize)
{
	if (!check_cipher_init(handle)) {
		rts_crypto_error("not initialized\n");
		return -RTS_E_CIPHER_NOT_INITIALIZED;
	}

	if (!handle || !ivsize) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	*ivsize = handle->tfm->cipher_info.ivsize;

	return 0;
}

int rts_cipher_setiv(struct rts_cipher_handle *handle,
			const uint8_t *iv, uint32_t ivsize)
{
	if (!check_cipher_init(handle)) {
		rts_crypto_error("not initialized\n");
		return -RTS_E_CIPHER_NOT_INITIALIZED;
	}

	if (ivsize != handle->tfm->cipher_info.ivsize) {
		rts_crypto_error("iv size is invalid\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	if ((ivsize && !iv) || (!ivsize && iv)) {
		rts_crypto_error("invalid parameters\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	if (ivsize && iv) {
		if (!handle->iv) {
			rts_crypto_error("not initialized\n");
			return -RTS_E_CIPHER_NOT_INITIALIZED;
		}

		memcpy(handle->iv, iv, ivsize);
	}

	return 0;
}

int rts_cipher_encrypt(struct rts_cipher_handle *handle,
			const uint8_t *in, uint32_t inlen,
			uint8_t *out, uint32_t *outlen)
{
	int ret;
	uint32_t blocksize;

	if (!check_cipher_init(handle)) {
		rts_crypto_error("not initialized\n");
		return -RTS_E_CIPHER_NOT_INITIALIZED;
	}

	if (!in || !out || !outlen) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	if (inlen > RTS_CIPHER_ENCRYPT_LEN_MAX) {
		rts_crypto_error("inlen is greater than max len\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	blocksize = handle->tfm->cipher_info.blocksize;

	if (inlen < 1) {
		rts_crypto_error("inlen is less than 1 byte\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	if (handle->padding_mode == RTS_CIPHER_PADDING_NONE) {
		if (is_need_padding(handle) && inlen % blocksize) {
			rts_crypto_error(
				"inlen is not multiple of block size\n");
			return -RTS_E_CIPHER_INVALID_ARG;
		}

		if (*outlen < inlen) {
			rts_crypto_error("outlen is less than inlen\n");
			return -RTS_E_CIPHER_INVALID_ARG;
		}
	} else {
		if (*outlen < (inlen / blocksize + 1) * blocksize) {
			rts_crypto_error("outlen is invalid\n");
			return -RTS_E_CIPHER_INVALID_ARG;
		}
	}

	if (g_keymode == RTS_CIPHER_KEYMODE_EFUSE)
		ret = __rts_setkey(handle, g_key, sizeof(g_key));

	ret = __rts_crypt_spliter(handle, in, inlen, out, outlen,
				ALG_OP_ENCRYPT);
	if (ret) {
		rts_crypto_error("__rts_crypt_spliter failed\n");
		return ret;
	}

	return 0;
}

int rts_cipher_decrypt(struct rts_cipher_handle *handle,
			const uint8_t *in, uint32_t inlen,
			uint8_t *out, uint32_t *outlen)
{
	int ret;
	uint32_t blocksize;

	if (!check_cipher_init(handle)) {
		rts_crypto_error("not initialized\n");
		return -RTS_E_CIPHER_NOT_INITIALIZED;
	}

	if (!in || !out || !outlen) {
		rts_crypto_error("null pointer\n");
		return -RTS_E_CIPHER_NULL_POINT;
	}

	blocksize = handle->tfm->cipher_info.blocksize;

	if (inlen < 1) {
		rts_crypto_error("inlen is less than 1 byte\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	if (*outlen < inlen) {
		rts_crypto_error("outlen is iless than inlen\n");
		return -RTS_E_CIPHER_INVALID_ARG;
	}

	if (handle->padding_mode == RTS_CIPHER_PADDING_NONE) {
		if (inlen > RTS_CIPHER_ENCRYPT_LEN_MAX) {
			rts_crypto_error(
				"inlen is greater than max len\n");
			return -RTS_E_CIPHER_INVALID_ARG;
		}

		if (is_need_padding(handle) && inlen % blocksize) {
			rts_crypto_error(
				"inlen is not multiple of block size\n");
			return -RTS_E_CIPHER_INVALID_ARG;
		}
	} else {
		if (inlen > RTS_CIPHER_ENCRYPT_LEN_MAX + blocksize) {
			rts_crypto_error(
				"inlen is greater than max+blocksize len\n");
			return -RTS_E_CIPHER_INVALID_ARG;
		}

		if (inlen % blocksize) {
			rts_crypto_error(
				"inlen is not multiple of block size\n");
			return -RTS_E_CIPHER_INVALID_ARG;
		}
	}

	if (g_keymode == RTS_CIPHER_KEYMODE_EFUSE)
		ret = __rts_setkey(handle, g_key, sizeof(g_key));

	ret = __rts_crypt_spliter(handle, in, inlen, out, outlen,
				ALG_OP_DECRYPT);
	if (ret) {
		rts_crypto_error("__rts_crypt_spliter failed\n");
		return ret;
	}

	return 0;
}

int rts_cipher_set_padding(struct rts_cipher_handle *handle,
			enum RTS_CIPHER_PADDING_MODE mode)
{
	if (!check_cipher_init(handle)) {
		rts_crypto_error("not initialized\n");
		return -RTS_E_CIPHER_NOT_INITIALIZED;
	}

	if (mode != RTS_CIPHER_PADDING_NONE && !is_need_padding(handle)) {
		rts_crypto_error("don't need padding\n");
		return -RTS_E_CIPHER_NOT_SUPPORT;
	}

	handle->padding_mode = mode;

	return 0;
}
