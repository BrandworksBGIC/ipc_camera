#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
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
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
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

#if 1 // use hw engine
    ENGINE_load_builtin_engines();
#endif

#if 1
    printf("key:\n");
    BIO_dump_fp(stdout, (const char*)key, sizeof(key));

    printf("plaintext:\n");
    BIO_dump_fp(stdout, (const char*)plaintext, strlen((char*)plaintext));
#endif

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

#if 1
    BIO_dump_fp(stdout, (const char*)decryptedtext, decryptedtext_len);
#endif

    return 0;
}

#endif

void ipc_aes_service_init(void)
{
    OPENSSL_init_crypto(OPENSSL_INIT_ENGINE_ALL_BUILTIN, NULL);

}

void ipc_aes_init_ctx_iv(struct ipc_aes_ctx* ctx, const uint8_t* key, const uint8_t* iv)
{
    memcpy(ctx->key, key, 32);
    memcpy(ctx->iv, iv, 16);
}

int ipc_aes_cbc_encrypt_buffer(struct ipc_aes_ctx* aes_ctx, unsigned char* plaintext, int plaintext_len)
{
    EVP_CIPHER_CTX* ctx;

    int len;

    int ciphertext_len;

    unsigned char* ciphertext = plaintext;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    /*
     * Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_ctx->key, aes_ctx->iv)) {
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }


    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    ciphertext_len = len;

    /*
     * Finalise the encryption. Further ciphertext bytes may be written at
     * this stage.
     */
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}

int ipc_aes_cbc_decrypt_buffer(struct ipc_aes_ctx* aes_ctx, unsigned char* ciphertext, int ciphertext_len)
{
    EVP_CIPHER_CTX* ctx;

    int len;

    int plaintext_len;
    unsigned char* plaintext = ciphertext;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    /*
     * Initialise the decryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_ctx->key, aes_ctx->iv)) {
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary.
     */
    if (1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) {
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    plaintext_len = len;

    /*
     * Finalise the decryption. Further plaintext bytes may be written at
     * this stage.
     */
    if (1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) {
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}
