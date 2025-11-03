#ifdef IPC_IS_UBOOT_BUILD
#include "linux/string.h"
#else
#include_next <string.h>
#endif