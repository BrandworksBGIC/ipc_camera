#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import sys

add_config = "include/generated/map.h"
f = open(add_config, "w")

f.write("""#ifdef CONFIG_DUAL_KERNEL_LOAD_CHECK
""")
f.write("#define RTS_KERNELLOAD_CHECK\t\\\n")
with open("./scripts/bootdual.sh", "r") as dual_fr:
    for dual_line in dual_fr.readlines():
        if dual_line.strip()=="":
            continue
        dual_line=dual_line.strip('\n')
        f.write("\t\t\t\"%s\"\t\\\n" % (dual_line))
f.write("\n")

f.write("#define RTS_BOOTCMD\t\t\"run bootcheck\"")
f.write("\n")
f.write("""#endif
""")
f.close()
