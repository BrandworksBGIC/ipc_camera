#include "ipc_uboot_port.h"

#include <asm/io.h>
#include <common.h>
#include <div64.h>
#include <image.h>
#include <malloc.h>
#include <spi_flash.h>

#include <mmc.h>
#include <stdio.h>

#ifndef CONFIG_SF_DEFAULT_SPEED
#define CONFIG_SF_DEFAULT_SPEED 10000000
#endif
#ifndef CONFIG_SF_DEFAULT_MODE
#define CONFIG_SF_DEFAULT_MODE SPI_MODE_3
#endif
#ifndef CONFIG_SF_DEFAULT_CS
#define CONFIG_SF_DEFAULT_CS 0
#endif
#ifndef CONFIG_SF_DEFAULT_BUS
#define CONFIG_SF_DEFAULT_BUS 0
#endif

static struct spi_flash* _g_cp_flash;

/**
 * This function takes a byte length and a delta unit of time to compute the
 * approximate bytes per second
 *
 * @param len		amount of bytes currently processed
 * @param start_ms	start time of processing in ms
 * @return bytes per second if OK, 0 on error
 */
static ulong bytes_per_second(unsigned int len, ulong start_ms)
{
    /* less accurate but avoids overflow */
    if (len >= ((unsigned int)-1) / 1024)
        return len / (max_t(ulong, get_timer(start_ms) / 1024, 1));
    else
        return 1024 * len / max_t(ulong, get_timer(start_ms), 1);
}

/**
 * Write a block of data to SPI flash, first checking if it is different from
 * what is already there.
 *
 * If the data being written is the same, then *skipped is incremented by len.
 *
 * @param flash		flash context pointer
 * @param offset	flash offset to write
 * @param len		number of bytes to write
 * @param buf		buffer to write from
 * @param cmp_buf	read buffer to use to compare data
 * @param skipped	Count of skipped data (incremented by this function)
 * @return NULL if OK, else a string containing the stage which failed
 */
static const char* spi_flash_update_block(struct spi_flash* flash, u32 offset, size_t len, const char* buf,
                                          char* cmp_buf, size_t* skipped)
{
    debug("offset=%#x, sector_size=%#x, len=%#zx\n", offset, flash->sector_size, len);
    if (spi_flash_read(flash, offset, len, cmp_buf))
        return "read";
    if (memcmp(cmp_buf, buf, len) == 0) {
        debug("Skip region %x size %zx: no change\n", offset, len);
        *skipped += len;
        return NULL;
    }
    if (spi_flash_erase(flash, offset, flash->sector_size))
        return "erase";
    if (spi_flash_write(flash, offset, len, buf))
        return "write";
    return NULL;
}

/**
 * Update an area of SPI flash by erasing and writing any blocks which need
 * to change. Existing blocks with the correct data are left unchanged.
 *
 * @param flash		flash context pointer
 * @param offset	flash offset to write
 * @param len		number of bytes to write
 * @param buf		buffer to write from
 * @return 0 if ok, 1 on error
 */
static int _spi_flash_update(struct spi_flash* flash, u32 offset, size_t len, const char* buf)
{
    const char* err_oper = NULL;
    char* cmp_buf;
    const char* end = buf + len;
    size_t todo;                /* number of bytes to do in this pass */
    size_t skipped         = 0; /* statistics */
    const ulong start_time = get_timer(0);
    size_t scale           = 1;
    const char* start_buf  = buf;
    ulong delta;

    if (end - buf >= 200)
        scale = (end - buf) / 100;
    cmp_buf = malloc(flash->sector_size);
    if (!cmp_buf) {
        err_oper = "malloc";
        goto exit;
    }
    ulong last_update = get_timer(0);

    for (; buf < end && !err_oper; buf += todo, offset += todo) {
        todo = min_t(size_t, end - buf, flash->sector_size);
        if (get_timer(last_update) > 100) {
            printf("   \rUpdating, %zu%% %lu B/s", 100 - (end - buf) / scale,
                   bytes_per_second(buf - start_buf, start_time));
            last_update = get_timer(0);
        }
        err_oper = spi_flash_update_block(flash, offset, todo, buf, cmp_buf, &skipped);
    }

    free(cmp_buf);
exit:
    putc('\r');
    if (err_oper) {
        printf("SPI flash failed in %s step\n", err_oper);
        return -1;
    }

    delta = get_timer(start_time);
    printf("%zu bytes written, %zu bytes skipped", len - skipped, skipped);
    printf(" in %ld.%lds, speed %ld B/s\n", delta / 1000, delta % 1000, bytes_per_second(len, start_time));

    return len - skipped;
}

int ipc_spi_flash_init(void)
{
    _g_cp_flash
        = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS, CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE);
    if (!_g_cp_flash) {
        printf("Failed to initialize SPI flash at %u:%u\n", CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS);
        return -1;
    }

    _g_cp_flash->flash_unlock(_g_cp_flash, 0, 0);

    return 0;
}

int ipc_spi_flash_read(unsigned int flash_addr, void* buffer, int read_len)
{
    return spi_flash_read(_g_cp_flash, flash_addr, read_len, buffer);
}

int ipc_spi_flash_erase(unsigned int flash_addr, int erase_len)
{
    return spi_flash_erase(_g_cp_flash, flash_addr, erase_len);
}

int ipc_spi_flash_write(unsigned int flash_addr, void* data, int data_len)
{
    printf("writeto:%x\n", flash_addr);
    return spi_flash_write(_g_cp_flash, flash_addr, data_len, data);
}

int ipc_spi_flash_update(unsigned int flash_addr, void* data, int data_len)
{
    printf("writeto:%x\n", flash_addr);
    return _spi_flash_update(_g_cp_flash, flash_addr, data_len, data);
}

static struct mmc* _g_mmc;

static struct mmc* _init_mmc_device(int dev, bool force_init)
{
    struct mmc* mmc;
    mmc = find_mmc_device(dev);
    if (!mmc) {
        printf("no mmc device at slot %x\n", dev);
        return NULL;
    }

    if (!mmc_getcd(mmc))
        force_init = true;

    if (force_init)
        mmc->has_init = 0;
    if (mmc_init(mmc))
        return NULL;

    return mmc;
}

int ipc_mmc_init(void)
{
    _g_mmc = _init_mmc_device(0, false);
    if (!_g_mmc)
        return -1;

    return 0;
}

int ipc_mmc_read(u64 sector, u32 count, void* buffer)
{
    int n = 0;
    n = blk_dread(mmc_get_blk_desc(_g_mmc), sector, count, buffer);

    return n;
}

char* __attribute__((weak)) envget(const char* key)
{
    printf(__func__);
    printf("weak\n");
    return NULL;
}

char* __attribute__((weak)) getenv(const char* key)
{
    printf(__func__);
    printf("weak\n");
    return NULL;
}

char* __attribute__((weak)) env_get(const char* key)
{
    printf(__func__);
    printf("weak\n");
    return NULL;
}

char* ipc_env_get(char* key)
{
    char* data = NULL;
    data       = envget(key);
    if (data) {
        return data;
    }

    data = getenv(key);
    if (data) {
        return data;
    }

    data = env_get(key);
    if (data) {
        return data;
    }

    return NULL;
}