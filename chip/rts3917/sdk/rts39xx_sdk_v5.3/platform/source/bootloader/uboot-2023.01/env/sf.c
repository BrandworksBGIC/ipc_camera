// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 *
 * (C) Copyright 2008 Atmel Corporation
 */
#include <common.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include <spi.h>
#include <spi_flash.h>
#include <search.h>
#include <errno.h>
#include <uuid.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include <u-boot/crc.h>
#include <cpu_func.h>

#define	OFFSET_INVALID		(~(u32)0)

#ifdef CONFIG_ENV_OFFSET_REDUND
#define ENV_OFFSET_REDUND	CONFIG_ENV_OFFSET_REDUND

static ulong env_offset		= CONFIG_ENV_OFFSET;
static ulong env_new_offset	= CONFIG_ENV_OFFSET_REDUND;

#else

#define ENV_OFFSET_REDUND	OFFSET_INVALID

#endif /* CONFIG_ENV_OFFSET_REDUND */

DECLARE_GLOBAL_DATA_PTR;

static int setup_flash_device(struct spi_flash **env_flash)
{
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct udevice *new;
	int	ret;

	/* speed and mode will be read from DT */
	ret = spi_flash_probe_bus_cs(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
				     &new);
	if (ret) {
		env_set_default("spi_flash_probe_bus_cs() failed", 0);
		return ret;
	}

	*env_flash = dev_get_uclass_priv(new);

#ifdef CONFIG_RTS_QSPI
	flash = *env_flash;
#endif

#else
	*env_flash = spi_flash_probe(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
				     CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);
	if (!*env_flash) {
		env_set_default("spi_flash_probe() failed", 0);
		return -EIO;
	}
#endif
	return 0;
}

static void self_copy(void)
{
	u8 ret = 0;
	u32 magic_num;
	u32 magic_num2;

	magic_num = htonl(REG32(RESCUE_UBOOT1_2_LOAD_ADDR));
	if (magic_num != 0x626f6f74) {
		printf("user write uboot to flash\n");

		flash->flash_unlock(flash, 0, 0);
		ret = spi_flash_erase(flash, RESCUE_FLASH_OFFSET,
				      RESCUE_BIN_LENGTH);
		printf("SF: %zu bytes @ %#x Erased: %s\n",
		       (size_t)RESCUE_BIN_LENGTH,
		       (u32)RESCUE_FLASH_OFFSET, ret ? "ERROR" : "OK");
		if (ret)
			return;

		ret = spi_flash_write(flash, RESCUE_FLASH_OFFSET,
				      RESCUE_UBOOT1_2_LOAD_SIZE,
				      (void *)RESCUE_UBOOT1_2_LOAD_ADDR);
		if (ret) {
			printf("SF: %zu bytes @ %#x %s: %s\n",
			       (size_t)RESCUE_BIN_LENGTH,
			       (u32)RESCUE_FLASH_OFFSET,
			       "Written", "ERROR");
			return;
		}
		magic_num2 = REG32(RESCUE_UBOOT3_LOAD_ADDR);
		if (magic_num != 0x10064AA || magic_num2 == 0) {
			ret = spi_flash_write(flash, RESCUE_UBOOT3_FLASH_OFFSET,
					      RESCUE_UBOOT3_LOAD_SIZE,
					      (void *)RESCUE_UBOOT3_LOAD_ADDR);
		} else {
			magic_num2 = REG32(0x19000020);
			ret = spi_flash_write(flash, RESCUE_UBOOT3_FLASH_OFFSET,
				RESCUE_UBOOT3_LOAD_SIZE,
				(void *)(RESCUE_UBOOT3_LOAD_ADDR - magic_num2));
		}
		printf("SF: %zu bytes @ %#x %s: %s\n",
			(size_t)RESCUE_BIN_LENGTH,
			(u32)RESCUE_FLASH_OFFSET,
			"Written", ret ? "ERROR" : "OK");
		flash->flash_lock(flash, 0, 0);
	} else {
		printf("user write linux to flash\n");

		memcpy((void *)(RESCUE_LINUX_LOAD_ADDR),
			(void *)RESCUE_UBOOT1_2_LOAD_ADDR,
			RESCUE_UBOOT1_2_LOAD_SIZE);
		flush_cache(RESCUE_LINUX_LOAD_ADDR & ~(ARCH_DMA_MINALIGN - 1),
			    RESCUE_UBOOT1_2_LOAD_SIZE);
		ret = do_write_for_rescure();
		printf("self_copy %s\n", ret ? "ERROR" : "OK");
	}
}

#if defined(CONFIG_ENV_OFFSET_REDUND)
static int env_sf_save(void)
{
	env_t	env_new;
	char	*saved_buffer = NULL, flag = ENV_REDUND_OBSOLETE;
	u32	saved_size = 0, saved_offset = 0, sector;
	u32	sect_size = CONFIG_ENV_SECT_SIZE;
	int	ret;
	struct spi_flash *env_flash;

	ret = setup_flash_device(&env_flash);
	if (ret)
		return ret;

	if (IS_ENABLED(CONFIG_ENV_SECT_SIZE_AUTO))
		sect_size = env_flash->mtd.erasesize;

	ret = env_export(&env_new);
	if (ret)
		return -EIO;
	env_new.flags	= ENV_REDUND_ACTIVE;

	if (gd->env_valid == ENV_VALID) {
		env_new_offset = CONFIG_ENV_OFFSET_REDUND;
		env_offset = CONFIG_ENV_OFFSET;
	} else {
		env_new_offset = CONFIG_ENV_OFFSET;
		env_offset = CONFIG_ENV_OFFSET_REDUND;
	}

	/* Is the sector larger than the env (i.e. embedded) */
	if (sect_size > CONFIG_ENV_SIZE) {
		saved_size = sect_size - CONFIG_ENV_SIZE;
		saved_offset = env_new_offset + CONFIG_ENV_SIZE;
		saved_buffer = memalign(ARCH_DMA_MINALIGN, saved_size);
		if (!saved_buffer) {
			ret = -ENOMEM;
			goto done;
		}
		ret = spi_flash_read(env_flash, saved_offset,
					saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	sector = DIV_ROUND_UP(CONFIG_ENV_SIZE, sect_size);

	puts("Erasing SPI flash...");
	env_flash->flash_unlock(env_flash, 0, 0);
	ret = spi_flash_erase(env_flash, env_new_offset,
				sector * sect_size);
	if (ret)
		goto done;

	puts("Writing to SPI flash...");

	ret = spi_flash_write(env_flash, env_new_offset,
		CONFIG_ENV_SIZE, &env_new);
	if (ret)
		goto done;

	if (sect_size > CONFIG_ENV_SIZE) {
		ret = spi_flash_write(env_flash, saved_offset,
					saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	ret = spi_flash_write(env_flash, env_offset + offsetof(env_t, flags),
				sizeof(env_new.flags), &flag);
	if (ret)
		goto done;

	puts("done\n");

	gd->env_valid = gd->env_valid == ENV_REDUND ? ENV_VALID : ENV_REDUND;

	printf("Valid environment: %d\n", (int)gd->env_valid);

done:
	env_flash->flash_lock(env_flash, 0, 0);
	spi_flash_free(env_flash);

	if (saved_buffer)
		free(saved_buffer);

	return ret;
}

static int env_sf_load(void)
{
	int ret;
	int read1_fail, read2_fail;
	env_t *tmp_env1, *tmp_env2;
	struct spi_flash *env_flash;
	u8 board_reset_mode = get_rst_mode();

	if (board_reset_mode == RES_MODE) {
		/* open nor flash control */
		ret = REG32(SYS_DMY_0);
		REG32(SYS_DMY_0) = ret | NOR_FLASH_CTRL;
	}

	tmp_env1 = (env_t *)memalign(ARCH_DMA_MINALIGN,
			CONFIG_ENV_SIZE);
	tmp_env2 = (env_t *)memalign(ARCH_DMA_MINALIGN,
			CONFIG_ENV_SIZE);
	if (!tmp_env1 || !tmp_env2) {
		env_set_default("malloc() failed", 0);
		ret = -EIO;
		goto out;
	}

	ret = setup_flash_device(&env_flash);
	if (ret)
		goto out;

	read1_fail = spi_flash_read(env_flash, CONFIG_ENV_OFFSET,
				    CONFIG_ENV_SIZE, tmp_env1);
	read2_fail = spi_flash_read(env_flash, CONFIG_ENV_OFFSET_REDUND,
				    CONFIG_ENV_SIZE, tmp_env2);

	ret = env_import_redund((char *)tmp_env1, read1_fail, (char *)tmp_env2,
				read2_fail, H_EXTERNAL);

	if (board_reset_mode == RES_MODE)
		self_copy();

	spi_flash_free(env_flash);
out:
	free(tmp_env1);
	free(tmp_env2);

	return ret;
}
#else
static int env_sf_save(void)
{
	u32	saved_size = 0, saved_offset = 0, sector;
	u32	sect_size = CONFIG_ENV_SECT_SIZE;
	char	*saved_buffer = NULL;
	int	ret = 1;
	env_t	env_new;
	struct spi_flash *env_flash;

	ret = setup_flash_device(&env_flash);
	if (ret)
		return ret;

	if (IS_ENABLED(CONFIG_ENV_SECT_SIZE_AUTO))
		sect_size = env_flash->mtd.erasesize;

	/* Is the sector larger than the env (i.e. embedded) */
	if (sect_size > CONFIG_ENV_SIZE) {
		saved_size = sect_size - CONFIG_ENV_SIZE;
		saved_offset = CONFIG_ENV_OFFSET + CONFIG_ENV_SIZE;
		saved_buffer = malloc(saved_size);
		if (!saved_buffer)
			goto done;

		ret = spi_flash_read(env_flash, saved_offset,
			saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	ret = env_export(&env_new);
	if (ret)
		goto done;

	sector = DIV_ROUND_UP(CONFIG_ENV_SIZE, sect_size);

	puts("Erasing SPI flash...");
	flash->flash_unlock(flash, 0, 0);
	ret = spi_flash_erase(env_flash, CONFIG_ENV_OFFSET,
		sector * sect_size);
	if (ret)
		goto done;

	puts("Writing to SPI flash...");
	ret = spi_flash_write(env_flash, CONFIG_ENV_OFFSET,
		CONFIG_ENV_SIZE, &env_new);
	if (ret)
		goto done;

	if (sect_size > CONFIG_ENV_SIZE) {
		ret = spi_flash_write(env_flash, saved_offset,
			saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	flash->flash_lock(flash, 0, 0);
	ret = 0;
	puts("done\n");

done:
	flash->flash_lock(flash, 0, 0);
	spi_flash_free(env_flash);

	if (saved_buffer)
		free(saved_buffer);

	return ret;
}

static int env_sf_load(void)
{
	int ret;
	char *buf = NULL;
	struct spi_flash *env_flash;
	u8 board_reset_mode = get_rst_mode();

	if (board_reset_mode == RES_MODE) {
		/* open nor flash control */
		ret = REG32(SYS_DMY_0);
		REG32(SYS_DMY_0) = ret | NOR_FLASH_CTRL;
	}

	buf = (char *)memalign(ARCH_DMA_MINALIGN, CONFIG_ENV_SIZE);
	if (!buf) {
		env_set_default("malloc() failed", 0);
		return -EIO;
	}

	ret = setup_flash_device(&env_flash);
	if (ret)
		goto out;

	ret = spi_flash_read(env_flash,
		CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, buf);
	if (ret) {
		env_set_default("spi_flash_read() failed", 0);
		goto err_read;
	}

	ret = env_import(buf, 1, H_EXTERNAL);
	if (!ret)
		gd->env_valid = ENV_VALID;

	if (board_reset_mode == RES_MODE)
		self_copy();

err_read:
	spi_flash_free(env_flash);
out:
	free(buf);

	return ret;
}
#endif

static int env_sf_erase(void)
{
	int ret;
	env_t env;
	struct spi_flash *env_flash;

	ret = setup_flash_device(&env_flash);
	if (ret)
		return ret;

	memset(&env, 0, sizeof(env_t));
	ret = spi_flash_write(env_flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, &env);
	if (ret)
		goto done;

	if (ENV_OFFSET_REDUND != OFFSET_INVALID)
		ret = spi_flash_write(env_flash, ENV_OFFSET_REDUND, CONFIG_ENV_SIZE, &env);

done:
	spi_flash_free(env_flash);

	return ret;
}

__weak void *env_sf_get_env_addr(void)
{
#ifndef CONFIG_SPL_BUILD
	return (void *)CONFIG_ENV_ADDR;
#else
	return NULL;
#endif
}

/*
 * check if Environment on CONFIG_ENV_ADDR is valid.
 */
static int env_sf_init_addr(void)
{
	env_t *env_ptr = (env_t *)env_sf_get_env_addr();

	if (!env_ptr)
		return -ENOENT;

	if (crc32(0, env_ptr->data, ENV_SIZE) == env_ptr->crc) {
		gd->env_addr = (ulong)&(env_ptr->data);
		gd->env_valid = ENV_VALID;
	} else {
		gd->env_valid = ENV_INVALID;
	}

	return 0;
}

#if defined(CONFIG_ENV_SPI_EARLY)
/*
 * early load environment from SPI flash (before relocation)
 * and check if it is valid.
 */
static int env_sf_init_early(void)
{
	int ret;
	int read1_fail;
	int read2_fail;
	int crc1_ok;
	env_t *tmp_env2 = NULL;
	env_t *tmp_env1;
	struct spi_flash *env_flash;

	/*
	 * if malloc is not ready yet, we cannot use
	 * this part yet.
	 */
	if (!gd->malloc_limit)
		return -ENOENT;

	tmp_env1 = (env_t *)memalign(ARCH_DMA_MINALIGN,
			CONFIG_ENV_SIZE);
	if (IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT))
		tmp_env2 = (env_t *)memalign(ARCH_DMA_MINALIGN,
					     CONFIG_ENV_SIZE);

	if (!tmp_env1 || !tmp_env2)
		goto out;

	ret = setup_flash_device(&env_flash);
	if (ret)
		goto out;

	read1_fail = spi_flash_read(env_flash, CONFIG_ENV_OFFSET,
				    CONFIG_ENV_SIZE, tmp_env1);

	if (IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT)) {
		read2_fail = spi_flash_read(env_flash,
					    CONFIG_ENV_OFFSET_REDUND,
					    CONFIG_ENV_SIZE, tmp_env2);
		ret = env_check_redund((char *)tmp_env1, read1_fail,
				       (char *)tmp_env2, read2_fail);

		if (ret < 0)
			goto err_read;

		if (gd->env_valid == ENV_VALID)
			gd->env_addr = (unsigned long)&tmp_env1->data;
		else
			gd->env_addr = (unsigned long)&tmp_env2->data;
	} else {
		if (read1_fail)
			goto err_read;

		crc1_ok = crc32(0, tmp_env1->data, ENV_SIZE) ==
				tmp_env1->crc;
		if (!crc1_ok)
			goto err_read;

		/* if valid -> this is our env */
		gd->env_valid = ENV_VALID;
		gd->env_addr = (unsigned long)&tmp_env1->data;
	}

	spi_flash_free(env_flash);

	return 0;
err_read:
	spi_flash_free(env_flash);

	free(tmp_env1);
	if (IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT))
		free(tmp_env2);
out:
	/* env is not valid. always return 0 */
	gd->env_valid = ENV_INVALID;
	return 0;
}
#endif

static int env_sf_init(void)
{
	int ret = env_sf_init_addr();
	if (ret != -ENOENT)
		return ret;
#ifdef CONFIG_ENV_SPI_EARLY
	return env_sf_init_early();
#endif
	/*
	 * return here -ENOENT, so env_init()
	 * can set the init bit and later if no
	 * other Environment storage is defined
	 * can set the default environment
	 */
	return -ENOENT;
}

U_BOOT_ENV_LOCATION(sf) = {
	.location	= ENVL_SPI_FLASH,
	ENV_NAME("SPIFlash")
	.load		= env_sf_load,
	.save		= ENV_SAVE_PTR(env_sf_save),
	.erase		= ENV_ERASE_PTR(env_sf_erase),
	.init		= env_sf_init,
};
