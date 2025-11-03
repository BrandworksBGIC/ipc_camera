#include <command.h>
#include <common.h>
#include <fdtdec.h>
#include <linux/ctype.h>
#include <malloc.h>
#include <menu.h>
#include <post.h>
#include <version.h>
#include <watchdog.h>

int __cp_get_password_auth(int abort)
{
    char key_buffer[8] = { 0 };
    int index = 0;
    int ret = 0;
    int timeout_count = 0;
    memset(key_buffer, 0, sizeof(key_buffer));

    while (abort) {
        if (tstc()) {
            key_buffer[index] = getc();
            if (key_buffer[index] == '\n' || key_buffer[index] == '\r') {
                if (index != 0) {
                    ret = strncmp(key_buffer, "^$@#@*&", 7);
                    if (ret == 0) {
                        printf("Password is correct\n");
                        break;
                    } else {
                        putc('\n');
                    }
                }
                index = 0;
            } else {
                putc('*');
                index++;
            }

            if (index >= sizeof(key_buffer)) {
                index = 0;
            }
        }
        udelay(10000);
        timeout_count++;
        if (timeout_count > 1000) {
            printf("wait password timeout 10s\n");
            ret = -1;
            break;
        }
    }

    return ret;
}
