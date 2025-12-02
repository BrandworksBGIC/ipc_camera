#include <kcapi.h>
#include <string.h>

#include "ipc_aes.h"

#if 0
int main(void)
{
    /*
     * Set up the key and iv. Do I need to say to not hard code these in a
     * real application? :-)
     */

    /* A 256 bit key */
    // unsigned char *key = (unsigned char *)"01234567890123456789012345678901";

#if 1 // use otp key 0
    unsigned char key[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else // assign real key
    unsigned char key[] = { 0x64, 0x43, 0x11, 0x0b, 0xba, 0x0d, 0xea, 0x3c, 0x9c, 0x71, 0x91, 0xf2, 0x20, 0x90, 0x80, 0x09,
                            0x7d, 0xe0, 0x2a, 0xb6, 0x51, 0x17, 0xc9, 0x3f, 0x69, 0x6c, 0x77, 0x55, 0x9a, 0x31, 0xa8, 0xa6 };
#endif

    /* A 128 bit IV */
    unsigned char* iv = (unsigned char*)"0123456789012345";

    /* Message to be encrypted */
    unsigned char* plaintext = (unsigned char*)"The quick brown fox jumps over the lazy dog";

    /*
     * Buffer for ciphertext. Ensure the buffer is long enough for the
     * ciphertext which may be longer than the plaintext, depending on the
     * algorithm and mode.
     */
    unsigned char ciphertext[128];

    /* Buffer for the decrypted text */
    unsigned char decryptedtext[128];

    int decryptedtext_len, ciphertext_len;

    /* Encrypt the plaintext */
    ciphertext_len = encrypt(plaintext, strlen((char*)plaintext), key, iv, ciphertext);

    /* Do something useful with the ciphertext here */
    printf("Ciphertext is:\n");
    BIO_dump_fp(stdout, (const char*)ciphertext, ciphertext_len);

    /* Decrypt the ciphertext */
    decryptedtext_len = decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext);

    /* Add a NULL terminator. We are expecting printable text */
    decryptedtext[decryptedtext_len] = '\0';

    /* Show the decrypted text */
    printf("Decrypted text is:\n");
    printf("%s\n", decryptedtext);

    BIO_dump_fp(stdout, (const char*)decryptedtext, decryptedtext_len);

    return 0;
}

#endif

void ipc_aes_service_init(void)
{
    /* kcapi does not require explicit initialization */
}

void ipc_aes_init_ctx_iv(struct ipc_aes_ctx* ctx, const uint8_t* key, const uint8_t* iv)
{
    memcpy(ctx->key, key, 32);
    memcpy(ctx->iv, iv, 16);
}

int ipc_aes_cbc_encrypt_buffer(struct ipc_aes_ctx* aes_ctx, unsigned char* plaintext, int plaintext_len)
{
    int ret;
    unsigned char* ciphertext = plaintext;
    int ciphertext_len        = plaintext_len;

    /* kcapi AES-CBC encryption requires data length to be multiple of 16 bytes */
    /* Since we disabled padding in OpenSSL, data should already be properly aligned */

    ret = kcapi_cipher_enc_aes_cbc(aes_ctx->key, 32, plaintext, ciphertext_len, aes_ctx->iv, ciphertext, ciphertext_len);
    if (ret < 0) {
        return -1;
    }

    return ciphertext_len;
}

int ipc_aes_cbc_decrypt_buffer(struct ipc_aes_ctx* aes_ctx, unsigned char* ciphertext, int ciphertext_len)
{
    int ret;
    unsigned char* plaintext = ciphertext;
    int plaintext_len        = ciphertext_len;

    /* kcapi AES-CBC decryption requires data length to be multiple of 16 bytes */
    /* Since we disabled padding in OpenSSL, data should already be properly aligned */

    ret = kcapi_cipher_dec_aes_cbc(aes_ctx->key, 32, ciphertext, plaintext_len, aes_ctx->iv, plaintext, plaintext_len);
    if (ret < 0) {
        return -1;
    }

    return plaintext_len;
}