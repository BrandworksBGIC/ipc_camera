#include <string.h>
#include <stdio.h>
#include <neuralyzer.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int err = 0;
	char mtddev[50];
	libmtd_t mtd_desc;
	struct mtd_dev_info mtd;
	int mtd_fd;
	int eb = 0;

	if (argc != 2) {
		printf("Usage: neucleanup /dev/mtdx(x=0,1...)\n"
			"Or: neucleanup --all\n");
		return -1;
	}

	mtd_desc = libmtd_open();
	if (!mtd_desc) {
		print_err("can't initialize libmtd\n");
		return -1;
	}

	if (global_check(mtd_desc) < 0) {
		printf("please set global partition as mtd0!\n");
		err = -1;
		goto fail;
	}

	if (!strcmp(argv[1], "--all"))
		sprintf(mtddev, "%s", "/dev/mtd0");
	else
		sprintf(mtddev, "%s", argv[1]);

	if (mtd_get_dev_info(mtd_desc, mtddev, &mtd) < 0) {
		print_err("mtd_get_dev_info failed\n");
		err = -1;
		goto fail;
	}

	mtd_fd = open(mtddev, O_RDWR);
	if (mtd_fd < 0) {
		print_err("open %s fail\n", mtddev);
		err = -1;
		goto fail;
	}

	for (eb = 0; eb < mtd.eb_cnt; eb++) {
		if (mtd.type == MTD_NANDFLASH) {
			err = mtd_is_bad(&mtd, mtd_fd, eb);
			if (err < 0) {
				print_err("MTD get bad block failed at 0x%x\n",
					eb * mtd.eb_size);
				close(mtd_fd);
				goto fail;
			} else if (err == 1) {
				print_dbg("Skip bad block at 0x%x\n",
					eb * mtd.eb_size);
				continue;
			}
		}

		err = mtd_erase(mtd_desc, &mtd, mtd_fd, eb);
		if (err < 0) {
			print_err("erase 0x%llx failed\n", (long long int)eb * mtd.eb_size);
			close(mtd_fd);
			goto fail;
		}
	}

	close(mtd_fd);
fail:
	libmtd_close(mtd_desc);
	return err;
}
