#include <dlfcn.h>
#include <pthread.h>

#include "ed25519.h"
#include "sha512.h"

#include "ipc_24c02.h"
#include "ipc_decrypt.h"
#include "ipc_factory.h"
#include "ipc_platform_api.h"

struct id_msg {
    u8  version;
    u8  signature[64];
    u64 ipc_device_id;
    u16 ipc_client_custom_id;
    u16 ipc_product_type;
    u16 cloud_platform_type;
    u8  cloud_platform_info_len;
    u8  cloud_platform_info[0];
} __attribute__((packed));

typedef struct id_msg* (*ipc_decrypt_f)(pu8 buffer, s32 buffer_len);
static ipc_decrypt_f _get_decrypt_func(pv8 so_file, vptr* _handle)
{
    v8 sign_file[256];
    snprintf(sign_file, sizeof(sign_file), "%s.sign", so_file);
    
    u8 sign[64] = {0};
    s32 ret = ipc_file_read_once(sign_file, (pv8)sign, sizeof(sign), __IPC_LOG__);
    if (ret < 0) return NULL;

    sha512_context sha = {0};
    sha512_init(&sha);

    u8 buff[512];
    s32 len = sizeof(buff);
    ITER_INIT(iter, 1);
    while(ipc_file_read_iter(iter[0], so_file, (pv8)buff, &len)) {
        sha512_update(&sha, buff, len);
    }

    ret = ipc_iter_retval(iter);
    if (ret < 0) return NULL;

    u8 sha512sum[64] = { 0 };
    sha512_final(&sha, sha512sum);

    u8 ed25519_pub[] = { 84, 172, 69, 158, 47, 171, 163, 1, 131, 37, 134, 208, 173, 144, 49, 88, 208, 204, 202, 152, 62, 235, 53, 77, 55, 147, 177, 117, 218, 39, 34, 152 };
    if (!ed25519_verify(sign, sha512sum, sizeof(sha512sum), ed25519_pub)) {
        ipcfatal("so sign error");
        return NULL;
    }

    vptr handle = dlopen(so_file, RTLD_LAZY);
    if (!handle) {
        ipcfatal("Dlopen error! errmsg=[%s]", dlerror());
        return NULL;
    }
    *_handle = handle;

    return (ipc_decrypt_f)dlsym(handle, "ipc_decode_id_decrypt_buffer");
}

#define DECRYPT_LIB_PATH "/app/lib/libipc_device_id_decryption.so"
#define DEV_NODE_24C02   "/dev/i2c-1"

static struct {
    ipc_decrypt_ininfo_t ininfo;
    ipc_decrypt_exinfo_t exinfo;
} *_g_decrypt_info;

static struct id_msg* _read_and_decrypt(pv8 dev_node, ipc_decrypt_f f_decrypt, pu8 buff, s32 len, u32 reg_width)
{
    s32 ret = 0;
    ret     = ipc_24c02_read(dev_node, buff, len, reg_width);
    if (ret < 0) {
        ipcdebug("read reg width=[%d] failed! retcode=[%d]", reg_width, ret);
        return NULL;
    }

    struct id_msg* msg = f_decrypt(buff, len);
    if (msg == NULL) ipcdebug("Decrypt failed!");

    return msg;
}

static s32 _decrypt(void)
{
    static u8 _alearly_decrypt = 0;
    if (_alearly_decrypt) return IPC_SUCCESS;

    clog_init("decrypt", "");

    u8 buff[4096] = {0};
    pv8 dev_node = DEV_NODE_24C02;
    v8 dev_node_buffer[256] = { 0 };
    struct id_msg* msg = NULL;
    if (ipc_plat_api(0)->misc_ctrl != NULL) {
        if (ipc_plat_api(0)->misc_ctrl(IPC_PLAT_MISC_CTRL_CMD_GET_24C02_DEV_NODE, NULL, dev_node_buffer) == 0) {
            dev_node = dev_node_buffer;
        }
    }

    vptr handle = NULL;
    ipc_decrypt_f f_decrypt = NULL;

    f_decrypt              = _get_decrypt_func(DECRYPT_LIB_PATH, &handle);
    if (f_decrypt == NULL) {
        ipcfatal("Get decrypt_func failed");
        if (handle) dlclose(handle);
        return IPC_VERIFY_FAILED;
    }

    s32 timeout_cnt = 0;
    do{
        if (((msg = _read_and_decrypt(dev_node, f_decrypt, buff, 256, 1)) == NULL)
            && ((msg = _read_and_decrypt(dev_node, f_decrypt, buff, 4096, 2)) == NULL)) {
            timeout_cnt++;
            ipcerror("Read and decrypt failed! retry times: %d\n", timeout_cnt);
        }

        if (timeout_cnt > 10) {
            ipcfatal("Read and decrypt failed!");
            dlclose(handle);
            return IPC_VERIFY_FAILED;
        }

        ipc_msleep(100);
    }while(NULL == msg);

    dlclose(handle);

    u16 cloud_platform_info_len = msg->cloud_platform_info_len;
    pu8 cloud_platform_info = msg->cloud_platform_info;
    if (cloud_platform_info_len == 0xff) {
        memcpy(&cloud_platform_info_len, msg->cloud_platform_info, sizeof(cloud_platform_info_len));
        cloud_platform_info += 2;
    }

    s32 decrypt_info_len = sizeof(*_g_decrypt_info) + cloud_platform_info_len + 1; 
    _g_decrypt_info = ipc_malloc(decrypt_info_len, decrypt_info_len);
    if (_g_decrypt_info == NULL) {
        ipcfatal("Malloc failed!");
        return IPC_NOMEM;
    }

    _g_decrypt_info->ininfo.device_id    = msg->ipc_device_id;
    _g_decrypt_info->ininfo.client_id    = msg->ipc_client_custom_id;
    _g_decrypt_info->ininfo.product_type = msg->ipc_product_type;

    _g_decrypt_info->exinfo.cloud_type   = msg->cloud_platform_type;
    _g_decrypt_info->exinfo.info_len     = cloud_platform_info_len;
    memcpy(_g_decrypt_info->exinfo.info_ctx, cloud_platform_info, cloud_platform_info_len);

    _alearly_decrypt = 1;

    return IPC_SUCCESS;
}

static volatile u8 _g_in_decrypt = 0;

ipc_decrypt_ininfo_p ipc_decrypt_ininfo(void)
{
    while (_g_in_decrypt) {
        ipc_msleep(100);
    }
    _g_in_decrypt = 1;

    s32 ret = _decrypt();

    _g_in_decrypt = 0;

    if (ret < 0)
        return NULL;

    return &_g_decrypt_info->ininfo;
}

ipc_decrypt_exinfo_p ipc_decrypt_exinfo(void)
{
    while (_g_in_decrypt) {
        ipc_msleep(100);
    }
    _g_in_decrypt = 1;

    s32 ret = _decrypt();

    _g_in_decrypt = 0;

    if (ret < 0)
        return NULL;

    return &_g_decrypt_info->exinfo;
}

#ifdef DECRYPT_TEST

s32 main(s32 argc, pv8 argv[])
{
    ipc_decrypt_exinfo();
}

#endif