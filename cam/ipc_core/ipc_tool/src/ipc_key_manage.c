/* Include system library for file access functions */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ipc_sha256.h" // Custom SHA-256 implementation

/* Include custom modules for key management, utilities, and file system operations */
#include "ipc_dfs.h"
#include "ipc_hex_bin.h"
#include "ipc_key_manage.h"
#include "ipc_misc.h"

/* Include encryption algorithm implementation */
#include "ipc_aes.h"

#include "ipc_std.h"

/* Define encryption key types */
typedef enum {
    KEY_TYPE_CONF_KEY_1, // Primary configuration encryption key identifier
    KEY_TYPE_CONF_KEY_2, // Second configuration encryption key identifier
    KEY_TYPE_NUM,        // Total count of key types (automatic counter)
} KEY_TYPE;

#define IPC_CONF_KEY_1_PATH_PREFIX "/conf/ipc/"
#define NEW_LEN(buf_len, align) ((buf_len) - (buf_len) % (align))

/* Global storage for encryption secrets */
static struct {
    u8 key[32];         // 256-bit encryption key storage
    u8 iv[16];          // 128-bit initialization vector storage
} _g_key[KEY_TYPE_NUM]; // Array holding all secret materials

static s32 get_random_bytes(pu8 buff, s32 len)
{
    int fd;
    int n  = 0;

    // Check parameter validity
    if (len <= 0 || !buff) {
        return 0;
    }
    // Optionally, you can add an upper bound check for len if needed
    // if (len > INT_MAX) { return 0; }

    fd = open("/dev/hwrng", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    while (n < len) {
        int to_read = len - n;
        // Prevent overflow
        if (to_read <= 0) {
            break;
        }
        /* coverity[INTEGER_OVERFLOW] */
        int r = read(fd, buff + n, to_read);
        if (r <= 0) { // read returns 0 or negative, exit to avoid infinite loop
            close(fd);
            return -1;
        }
        n += r;
        // Check for integer overflow
        if (n < 0) {
            close(fd);
            return -1;
        }
    }
    close(fd);
    /* coverity[INTEGER_OVERFLOW] */
    return n;
}

/* Encrypt source data using AES-256-CBC cipher with OTP-based key */
static s32 encrypt_data_with_otp_key(pu8 data, s32 data_len)
{

    struct ipc_aes_ctx ctx;

    unsigned char key[32];
    unsigned char iv[16];

    memset(key, 0, 32);
    memset(iv, 0, 16);

    key[31] = 0x1;
    iv[15]  = 0x1;

    ipc_aes_init_ctx_iv(&ctx, key, iv);

    return ipc_aes_cbc_encrypt_buffer(&ctx, data, data_len);
}

/* Decrypt data using AES-256-CBC cipher with OTP-derived credentials */
static s32 decrypt_data_with_otp_key(pu8 data, s32 data_len)
{

    struct ipc_aes_ctx ctx;

    unsigned char key[32];
    unsigned char iv[16];

    memset(key, 0, 32);
    memset(iv, 0, 16);

    key[31] = 0x1;
    iv[15]  = 0x1;

    ipc_aes_init_ctx_iv(&ctx, key, iv);

    return ipc_aes_cbc_decrypt_buffer(&ctx, data, data_len);
}

/* Manage configuration key lifecycle */
static s32 create_conf_key_1(void)
{
    // Check for existing encrypted key file

    u8 conf_key_1[64];
    s32 key_buf_len = 0;
    s32 ret         = 0;

    key_buf_len = ipc_file_read_once("/conf/conf_key_1", (pv8)conf_key_1, 64, __IPC_LOG__);

    if (key_buf_len > 0) {
        decrypt_data_with_otp_key(conf_key_1, key_buf_len);

        memcpy(_g_key[KEY_TYPE_CONF_KEY_1].key, conf_key_1, 32);
        memcpy(_g_key[KEY_TYPE_CONF_KEY_1].iv, conf_key_1 + 32, 16);

        // ipc_file_write_once("/tmp/aes_conf_1_key.bin", (pv8)conf_key_1, 64, __IPC_LOG__);
        // ipc_exec("md5sum /tmp/aes_conf_1_key.bin");

        return 0;
    }

    ret = get_random_bytes(conf_key_1, 64);
    if (ret < 0) {
        printf("Error get_random_bytes for conf_key_1\n");
        exit(-1);
        return -1;
    }

    memcpy(_g_key[KEY_TYPE_CONF_KEY_1].key, conf_key_1, 32);
    memcpy(_g_key[KEY_TYPE_CONF_KEY_1].iv, conf_key_1 + 32, 16);

    encrypt_data_with_otp_key(conf_key_1, 64);

    ipc_file_write_once("/conf/conf_key_1", (pv8)conf_key_1, 64, __IPC_LOG__);

    return 0;
}

static void try_create_root_key(void)
{
    FILE* fp           = NULL;
    u8 buffer[64]      = { 0 };
    v8 str_buffer[128] = { 0 };
    s32 ret            = 0;
    fp                 = fopen("/sys/bus/nvmem/devices/rts-otp0/nvmem", "r");
    if (fp == NULL) {
        printf("Error opening OTP device\n");
        exit(-1);
    }

    if (fread(buffer, 1, 1, fp) != 1) {
        printf("Error reading OTP device\n");
        fclose(fp);
        exit(-1);
    }
    fclose(fp);

    u8 flag = (~buffer[0]);

    printf("otp flag: %x\n", flag);

    if ((flag & 0x82)) {
        printf("otp is lock\n");
        return;
    }

    ret = get_random_bytes(buffer, 32);
    if (ret < 0) {
        printf("Error get_random_bytes\n");
        exit(-1);
    }

    memset(str_buffer, 0, 128);
    ipc_bin_to_hex(buffer, 32, (pv8)str_buffer, 128);

    // printf("otp random key: %s\n", str_buffer);

    ret = ipc_exec("otp_mfg -w --ecc %s 48", str_buffer);
    if (ret < 0) {
        printf("Error otp_mfg\n");
        exit(-1);
    }

    ret = get_random_bytes(buffer, 16);
    if (ret < 0) {
        printf("Error get_random_bytes\n");
        exit(-1);
    }

    memset(str_buffer, 0, 128);
    ipc_bin_to_hex(buffer, 16, (pv8)str_buffer, 128);

    // printf("otp random iv: %s\n", str_buffer);

    ret = ipc_exec("otp_mfg -w --ecc %s 240", str_buffer);
    if (ret < 0) {
        printf("Error otp_mfg\n");
        exit(-1);
    }
}

/* Data transformation handler */
static s32 run_decrypt_encrypt_data(KEY_TYPE key_type, u8 reverse, vptr buff, s32 len)
{
    // Function pointer for choosing encryption/decryption mode
    typedef int (*AES_CBC_x_buffer_t)(struct ipc_aes_ctx*, uint8_t*, int);

    if ((u32)key_type >= KEY_TYPE_NUM || !buff || len < 0) {
        return IPC_INVALID_ARGS;
    }

    AES_CBC_x_buffer_t aes_x = reverse ? ipc_aes_cbc_decrypt_buffer : ipc_aes_cbc_encrypt_buffer;
    s32 aligned_len = NEW_LEN(len, 16);
    if (aligned_len == 0) {
        return IPC_SUCCESS;
    }

    struct ipc_aes_ctx ctx;

    // A single AF_ALG request on this platform must stay below 180224 bytes.
    if (aligned_len > 180000) {
        aligned_len = 180000;
    }
    ipc_aes_init_ctx_iv(&ctx, _g_key[key_type].key, _g_key[key_type].iv);
    return aes_x(&ctx, buff, aligned_len) < 0 ? IPC_FAILED : IPC_SUCCESS;
}

/* Initialize key management system */
s32 key_manage_init(void)
{
    ipc_aes_service_init();

    try_create_root_key();

    // Generate or load configuration key 1
    create_conf_key_1();

    // Generate or load configuration key 2
    u8 conf_key_2[48];
    s32 key_buf_len = ipc_file_read_once("/conf/conf_key_2", (pv8)conf_key_2, 48, __IPC_LOG__);

    if (key_buf_len > 0) {
        // Decrypt and store in memory
        decrypt_data_with_otp_key(conf_key_2, key_buf_len);
        memcpy(_g_key[KEY_TYPE_CONF_KEY_2].key, conf_key_2, 32);
        memcpy(_g_key[KEY_TYPE_CONF_KEY_2].iv, conf_key_2 + 32, 16);

        // ipc_file_write_once("/tmp/conf_key_2", (pv8)conf_key_2, 64, __IPC_LOG__);
    }

    return 0;
}

/* Remove configuration key file */
s32 key_manage_delete_conf_key_1(void)
{
    return ipc_rm("/conf/conf_key_1");
}

/* Conditional data decryption */
s32 key_manage_decrypt_with_conf_key_1(pv8 path, pv8 data, s32 len)
{
    // Only process specific system paths
    if (!path
        || strncmp(path, IPC_CONF_KEY_1_PATH_PREFIX, sizeof(IPC_CONF_KEY_1_PATH_PREFIX) - 1) != 0) {
        return 0;
    }

    // Perform decryption
    return run_decrypt_encrypt_data(KEY_TYPE_CONF_KEY_1, 1, data, len);
}

/* Conditional data encryption */
s32 key_manage_encrypt_with_conf_key_1(pv8 path, pv8 data, s32 len)
{
    // Path validation check
    if (!path
        || strncmp(path, IPC_CONF_KEY_1_PATH_PREFIX, sizeof(IPC_CONF_KEY_1_PATH_PREFIX) - 1) != 0) {
        return 0;
    }

    // Perform encryption
    return run_decrypt_encrypt_data(KEY_TYPE_CONF_KEY_1, 0, data, len);
}

s32 key_manage_decrypt_with_root_key(pu8 data, s32 len)
{
    decrypt_data_with_otp_key(data, NEW_LEN(len, 16));

    return 0;
}

s32 key_manage_encrypt_with_root_key(pu8 data, s32 len)
{
    encrypt_data_with_otp_key(data, NEW_LEN(len, 16));

    return 0;
}
s32 key_manage_encrypt_with_conf_key_2(pu8 data, s32 len)
{
    run_decrypt_encrypt_data(KEY_TYPE_CONF_KEY_2, 0, data, len);
    return 0;
}

s32 key_manage_decrypt_with_conf_key_2(pu8 data, s32 len)
{
    run_decrypt_encrypt_data(KEY_TYPE_CONF_KEY_2, 1, data, len);
    return 0;
}

s32 key_manage_create_conf_key_2(pv8 seed)
{
    if (!seed) {
        printf("Error: Seed required for conf_key_2 generation\n");
        return -1;
    }

    u8 conf_key_2[48]   = { 0 };
    u8 existing_key[48] = { 0 };
    s32 key_buf_len     = 0;
    s32 need_update;

    // First try to read existing key
    key_buf_len = ipc_file_read_once("/conf/conf_key_2", (pv8)existing_key, 48, __IPC_LOG__);
    if (key_buf_len > 0) {
        decrypt_data_with_otp_key(existing_key, key_buf_len);
    }

    // Generate new key
    u8 md_value[32];
    ipc_sha256_ctx_t sha_ctx;

    // Derive key
    ipc_sha256_init(&sha_ctx);
    ipc_sha256_update(&sha_ctx, (u8*)seed, strlen(seed));
    ipc_sha256_final(&sha_ctx, md_value);
    memcpy(conf_key_2, md_value, 32);

    // Derive IV
    char iv_seed[256];
    snprintf(iv_seed, sizeof(iv_seed), "%s_IV", seed);
    ipc_sha256_init(&sha_ctx);
    ipc_sha256_update(&sha_ctx, (u8*)iv_seed, strlen(iv_seed));
    ipc_sha256_final(&sha_ctx, md_value);
    memcpy(conf_key_2 + 32, md_value, 16);

    // Compare with existing key if it exists
    if (key_buf_len > 0) {
        need_update = memcmp(conf_key_2, existing_key, sizeof(conf_key_2)) != 0;
        if (!need_update) {
            printf("conf_key_2 matches existing key - no update needed\n");
            return 0;
        }
        printf("conf_key_2 differs from existing key - updating\n");
    }

    // Store and persist key if different or new
    memcpy(_g_key[KEY_TYPE_CONF_KEY_2].key, conf_key_2, 32);
    memcpy(_g_key[KEY_TYPE_CONF_KEY_2].iv, conf_key_2 + 32, 16);

    encrypt_data_with_otp_key(conf_key_2, sizeof(conf_key_2));
    ipc_file_write_once("/conf/conf_key_2", (pv8)conf_key_2, sizeof(conf_key_2), __IPC_LOG__);

    return 0;
}

/* Remove configuration key file */
s32 key_manage_delete_conf_key_2(void)
{
    return ipc_rm("/conf/conf_key_2");
}
