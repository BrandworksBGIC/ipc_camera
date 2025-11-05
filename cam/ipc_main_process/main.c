#include <ipc_middleware_sal.h>
s32 main(void)
{
    printf("ipc " __CLOUD__ "_version:" __IPC_VERSION__ "\n");
    printf("ipc " __DATE__ " " __TIME__ "\n");
    return ipc_middleware_main_process(__IPC_VERSION__);
}
