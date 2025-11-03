#ifndef __IPC_UBOOT_PORT_H__
#define __IPC_UBOOT_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <command.h>
#include <common.h>

#include "string.h"

#if IPC_ARCH_PLATFORM_RTS3917N

#define IPC_ARCH_PLATFORM_NAME "rts3917n"

#elif IPC_ARCH_PLATFORM_RTS3918N

#define IPC_ARCH_PLATFORM_NAME "rts3918n"

#else
#error "add one platform name"
#endif

int ipc_spi_flash_init(void);
int ipc_spi_flash_read(unsigned int flash_addr, void* buffer, int read_len);
int ipc_spi_flash_erase(unsigned int flash_addr, int erase_len);
int ipc_spi_flash_write(unsigned int flash_addr, void* data, int data_len);
int ipc_spi_flash_update(unsigned int flash_addr, void* data, int data_len);


int ipc_mmc_init(void);
int ipc_mmc_read(u64 sector, u32 count, void* buffer);

char* ipc_env_get(char*key);

#ifdef __cplusplus
}
#endif

#endif //__CP_UBOOT_PORT_H__