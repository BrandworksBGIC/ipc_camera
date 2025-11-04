#ifndef _IPC_AES_H_
#define _IPC_AES_H_

#include <stdint.h>
#include <stddef.h>


struct ipc_aes_ctx
{
  uint8_t key[32];
  uint8_t iv[16];
};

void ipc_aes_service_init(void);

void ipc_aes_init_ctx_iv(struct ipc_aes_ctx* ctx, const uint8_t* key, const uint8_t* iv);

int ipc_aes_cbc_encrypt_buffer(struct ipc_aes_ctx* aes_ctx, unsigned char* plaintext, int plaintext_len);

int ipc_aes_cbc_decrypt_buffer(struct ipc_aes_ctx* aes_ctx, unsigned char* ciphertext, int ciphertext_len);

#endif // _AES_H_
