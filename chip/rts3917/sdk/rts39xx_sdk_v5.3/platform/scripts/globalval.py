#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import sys
import re


class globalval():
    def __init__(self, config_fname: str):
        self.ic_type: int = 0x0000
        self.dts_file: str
        self.dts_type: str
        self.__parse_config(config_fname)

    def __parse_config(self, config_fname: str):
        f = open(config_fname)
        content = f.read()
        f.close()

        # dts file
        tmp = ""
        dts_config = ""

        custom_dts_config = re.findall('BR2_LINUX_KERNEL_CUSTOM_DTS_PATH=.*', content)
        tmp = custom_dts_config[0].split('"')[1]
        if tmp:
            dts_config = (tmp + ' ').split('.dts ',1)[0]
            self.dts_file = os.path.basename(dts_config)
            self.dts_type = 'custom_dts_path'
        else:
            intree_dts_config = re.findall('BR2_LINUX_KERNEL_INTREE_DTS_NAME=.*', content)
            tmp = intree_dts_config[0].split('"')[1]
            if tmp:
                dts_config = tmp
                self.dts_file = os.path.basename(dts_config)
                self.dts_type = 'intree_dts_name'

        # ic type
        if (len(re.findall('BR2_CONFIG_HW_VER_RTS3917=y', content)) == 1):
            self.ic_type = 3917
        elif (len(re.findall('BR2_CONFIG_HW_VER_RTS3915=y', content)) == 1):
            self.ic_type = 3915
        elif (len(re.findall('BR2_CONFIG_HW_VER_RTS3913=y', content)) == 1):
            self.ic_type = 3913
        elif (len(re.findall('BR2_CONFIG_HW_VER_RTS3903=y', content)) == 1):
            self.ic_type = 3903


    def get_ic_type(self) -> int:
        return self.ic_type

    def get_dtsfile(self) -> str:
        return self.dts_file

    def get_dtbfile(self) -> str:
        return self.dts_file.split('.')[0] + '.dtb'

    def get_defconfig(self, linux_dir: str) -> str:
        if self.dts_file is not None:
            if (self.dts_type == 'custom_dts_path'):
                return linux_dir + '/arch/arm/boot/dts/partitions.dts'
            else:
                return linux_dir + '/arch/arm/boot/dts/realtek/partitions.dts'
