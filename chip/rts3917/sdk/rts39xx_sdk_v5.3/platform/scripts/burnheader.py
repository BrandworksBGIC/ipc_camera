#!/usr/bin/env python
# -*- coding:utf-8 -*-

import sys
import os
import re
import platform
import getpass
import datetime
import binascii
import struct
import merge


relpath_dict = {
    "merge_ini": "merge.ini",
    "target": "linux-custom/target",
    "sdkver": "../VERSION",
    "uboot_r": "uboot-custom/board/realtek/rlxboard/rlxboard.h",
    "uboot": "uboot-custom/Makefile",
    "uboot_c": "uboot-custom/.config",
    "sdk_c": "../.config",
    "kernel_v": "linux-custom/include/config/kernel.release",
}


def pathj(dname, fname):
    return os.path.abspath(os.path.join(dname, fname))

class Header_info:
    def __init__(self, output_directory, scripts_directory, user_fs, images_ini, config):
        self.magic = "7265616c"
        self.scripts_dir = scripts_directory
        self.output_dir = output_directory
        self.build_dir = output_directory + "/build"
        self.images_dir = output_directory + "/images"
        merge_ini = pathj(self.output_dir, relpath_dict["merge_ini"])
        self.pairs = images_ini
        self.f_target = self.init_fname("target")
        self.f_sdkver = self.script_fname("sdkver")
        self.f_uboot = self.init_fname("uboot")
        self.f_uboot_r = self.init_fname("uboot_r")
        self.f_mcu_fw = self.find_mcu_fw()
        self.f_uboot_c = self.init_fname("uboot_c")
        self.f_sdk_c = self.init_fname("sdk_c")
        self.f_kernel_ver = self.init_fname("kernel_v")

        self.lines = []
        print("-------------------- header begin --------------------")
        self.add_line(self.magic, "magic")
        self.add_line(self.bitmap_ic(), "mapic")
        self.add_line(self.mcu_pid_vid(), "pidvid")
        self.add_line(self.kernel_version(), "kernel")
        self.add_line(self.sdk_version(), "sdkver")
        self.add_line(self.mcu_trunk(), "trunk")
        self.add_line(self.mcu_customer(), "customer")
        self.add_line(self.uboot_version(), "uboot")
        # self.add_line(self.uboot_revision()+"0000", "uboot_r")
        self.add_line(self.signature(), "signature", False)
        self.add_line(self.build_date(), "date")
        self.add_line(self.build_time()+"00", "time")
        self.add_line(self.uboot_ddr()+"000000", "ddr")
        self.add_dual_partition(config, "kernel")
        self.add_dual_partition(config, "rootfs")
        self.add_line(self.boot_mode(), "boot_mode")
        self.add_line(self.boot_media(), "boot_media")
        self.add_line(self.board_type(), "board_type")
        self.add_line(self.align_zero_bytes(256))
        print("-------------------- header end   --------------------")

    def add_dual_partition(self, config, name):
        line = "{:016x}".format(config.config_map[name][4])
        self.add_line(line, name+'B')

    def find_mcu_fw(self):
        mcu_fw = [f for key, f in self.pairs if key == "mcu_fw"]
        if len(mcu_fw) != 1:
            return
        return mcu_fw[0]

    def parse_ddr_type(self):
        f = open(self.f_uboot_c)
        content = f.read()
        f.close()
        ddr_type = re.findall('CONFIG_DDR2_MCM=y', content)
        if len(ddr_type) == 1:
            return 0x10
        ddr_type = re.findall('CONFIG_QFN88_DDR2_512MBIT=y', content)
        if len(ddr_type) == 1:
            return 0x11
        ddr_type = re.findall('CONFIG_DDR3_GENERAL=y', content)
        if len(ddr_type) == 1:
            return 0x12
        ddr_type = re.findall('CONFIG_QFN88_DDR3_1GBIT=y', content)
        if len(ddr_type) == 1:
            return 0x13
        ddr_type = re.findall('CONFIG_BGA325_DDR3_GENERAL=y', content)
        if len(ddr_type) == 1:
            return 0x14
        ddr_type = re.findall('CONFIG_QFN88_DDR2_256MBIT=y', content)
        if len(ddr_type) == 1:
            return 0x15
        ddr_type = re.findall('BGA151_DDR2_512MBIT=y', content)
        if len(ddr_type) == 1:
            return 0x16
        return 0x00

    def find_ddr_type(self):
        uboot_f = [f for key, f in self.pairs if key == "boot"]
        if len(uboot_f) != 1:
            return 0

        return self.parse_ddr_type()

    def add_line(self, line, info="", tohex=True):
        if info:
            print("{:12}{}".format(info, line))
        if tohex:
            self.lines.append(binascii.a2b_hex(line))
        else:
            self.lines.append(line.encode())

    def parse_ic_type(self):
        f = open(self.f_sdk_c)
        content = f.read()
        f.close()
        ic_type = re.findall('BR2_CONFIG_HW_VER_RTS3917=y', content)
        if len(ic_type) == 1:
            return 3917
        ic_type = re.findall('BR2_CONFIG_HW_VER_RTS3915=y', content)
        if len(ic_type) == 1:
            return 3915
        ic_type = re.findall('BR2_CONFIG_HW_VER_RTS3913=y', content)
        if len(ic_type) == 1:
            return 3913
        ic_type = re.findall('BR2_CONFIG_HW_VER_RTS3903=y', content)
        if len(ic_type) == 1:
            return 3903
        return 0x0000

    def bitmap_ic(self):
        bitmap = 0
        for key, f in self.pairs:
            if key == "boot":
                bitmap += 1
            if key == "mcu_fw":
                bitmap += 2
            if key == "hconf":
                bitmap += 4
            if key == "userdata":
                bitmap += 8
            if key == "kernel":
                bitmap += 16
            if key == "rootfs":
                bitmap += 32
            if key == "ldc":
                bitmap += 64
            if key == "dtb":
                bitmap += 128
        ic = self.parse_ic_type()
        return "{:04x}{:04x}".format(ic, bitmap)

    def signature(self, size=32):
        user = getpass.getuser()
        host = platform.node()
        line = "{}@{}".format(user, host)
        line += size * "\0"
        return line[:size]

    def build_date(self):
        line = datetime.datetime.now().strftime("%Y-%m-%d")
        y, m, d = [int(x) for x in line.split("-")]
        return "{:04x}{:02x}{:02x}".format(y, m, d)

    def build_time(self):
        line = datetime.datetime.now().strftime("%H-%M-%S")
        h, m, s = [int(x) for x in line.split("-")]
        return "{:02x}{:02x}{:02x}".format(h, m, s)

    def uboot_ddr(self):
        ddr_type = self.find_ddr_type()
        return "{:02x}".format(ddr_type)

    def init_fname(self, key):
        return pathj(self.build_dir, relpath_dict[key])

    def script_fname(self, key):
        return pathj(self.scripts_dir, relpath_dict[key])

    # def kernel_version(self):
    #     return "{:02x}{:02x}{:02x}".format(3, 10, 27)

    def kernel_version(self):
        with open(self.f_kernel_ver) as f:
            linum = 0
            for line in f:
                linum += 1
                old_line = line
                line = line.strip()
                if not line:
                    continue
                V = []
                line = line.rstrip("+")
                for x in line.split("."):
                    if x.isdigit():
                        V.append(int(x))
                    else:
                        V.append(0)
                while len(V) < 3:
                    V.append(0)
                v1, v2, v3= V[:3]
                v3 = ((v3 & 0xFF) << 8) + ((v3 & 0xFF00) >> 8)
                return "{:02x}{:02x}{:04x}".format(v1, v2, v3)

    def sdk_version(self):
        with open(self.f_sdkver) as f:
            linum = 0
            for line in f:
                linum += 1
                old_line = line
                line = line.strip()
                if not line:
                    continue
                if line == "master":
                    return "{:02}{:02}{:02}{:02}".format(0, 0, 0, 0)
                elif line == "maintain":
                    return "{:02}{:02}{:02}{:02}".format(0, 0, 1, 0)
                rc = 0
                if "rc" in line:
                    line, rc = line.split("-rc")
                V = []
                for x in line[1:].split("."):
                    if x.isdigit():
                        V.append(int(x))
                    else:
                        V.append(0)
                while len(V) < 3:
                    V.append(0)
                v1, v2, v3= V[:3]
                rc = int(rc)
                return "{:02}{:02}{:02}{:02}".format(v1, v2, v3, rc)

    def uboot_revision(self):
        with open(self.f_uboot_r) as f:
            for line in f:
                line = line.strip()
                if "TAG_STRING" in line:
                    row = line.split('"')[1][1:].split(".")
                    row = "{:02x}{:02x}".format(int(row[0]), int(row[1]))
                    return row

    def uboot_version(self):
        ver, plevel, extra = 2014, 1, 2
        if not os.path.exists(self.f_uboot):
            print("WARNING: uboot makefile not exist")
            return "{:04x}{:02x}{:02x}".format(ver, plevel, extra)

        with open(self.f_uboot) as f:
            for line in f:
                if "VERSION" in line[:len("VERSION")]:
                    ver = int(line.split("=")[-1].strip())
                if "PATCHLEVEL" in line:
                    plevel = int(line.split("=")[-1].strip())
                if "EXTRAVERSION" in line:
                    if line.split("=")[-1].strip() == "":
                        extra = 0
                    else:
                        extra = int(line.split("-rc")[-1])
                    return "{:04x}{:02x}{:02x}".format(ver, plevel, extra)
        print("ERROR: uboot makefile parse failed")
        return "{:04x}{:02x}{:02x}".format(0, 0, 0)

    def mcu_pid_vid(self):
        offset = 0x1008
        if not self.f_mcu_fw:
            return "00" * 4
        with open(self.f_mcu_fw, "rb") as f:
            f.seek(offset)
            vid, pid = struct.unpack(">HH", f.read(4))
            row = "{:04x}{:04x}".format(vid, pid)
            return row

    def mcu_trunk(self):
        offset = 0x100c
        if not self.f_mcu_fw:
            return "00" * 4
        with open(self.f_mcu_fw, "rb") as f:
            f.seek(offset)
            trunk = struct.unpack(">I", f.read(4))[0]
            return "{:08x}".format(trunk)

    def mcu_customer(self):
        offset = 0x1010
        if not self.f_mcu_fw:
            return "00" * 4
        with open(self.f_mcu_fw, "rb") as f:
            f.seek(offset)
            customer = struct.unpack(">I", f.read(4))[0]
            return "{:08x}".format(customer)

    def align_zero_bytes(self, size):
        for line in self.lines:
            size -= len(line)
        return "00" * size

    def write_header(self, fd):
        for line in self.lines:
            fd.write(line)

    def boot_mode(self):
        f = open(self.f_sdk_c)
        content = f.read()
        f.close()
        crypt_boot = re.findall('BR2_CONFIG_CRYPTO_BOOT=y', content)
        secure_boot = re.findall('BR2_CONFIG_SECURE_BOOT=y', content)
        if len(secure_boot) == 1:
            if len(crypt_boot) == 1:
                return "{:02x}".format(3)
            else:
                return "{:02x}".format(2)
        else:
            if len(crypt_boot) == 1:
                return "{:02x}".format(1)
            else:
                return "{:02x}".format(0)
    def boot_media(self):
        f = open(self.f_uboot_c)
        content = f.read()
        f.close()
        if (re.findall('CONFIG_RTS_NOR_BOOT=y', content)):
            return "{:02}".format(0)
        if (re.findall('CONFIG_RTS_NAND_BOOT=y', content)):
            return "{:02}".format(1)
        if (re.findall('CONFIG_RTS_EMMC_BOOT=y', content)):
            return "{:02}".format(2)
    def board_type(self):
        f = open(self.f_sdk_c)
        for line in f:
            if "BR2_DEFCONFIG" in line:
                board = line.split("=")[-1].split("/")[-1].split("_")[0].split("rts")[-1]
                a, b, c = [0, 0, 0]
                for i in range(len(board)):
                    if i < 2:
                        a = a * 16 + int(board[i])
                    elif i < 4:
                        b = b * 16 + int(board[i])
                    elif i == 4:
                        c = ord(board[i])
                    else:
                        break;
        return "{:02x}{:02x}{:02x}".format(a, b, c)
