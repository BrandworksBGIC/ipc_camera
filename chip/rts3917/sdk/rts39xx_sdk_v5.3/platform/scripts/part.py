#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import sys
import argparse
from pydevicetree import Devicetree
from pydevicetree import Node
import re
from abc import ABC, abstractmethod
import build_verity_img
import configparser


class Secure_mode (ABC):
    rtskey_out_dir: str
    rtskey_target_dir: str

    @classmethod
    def create_object(cls, config_fname: str, stype: str, group: str, flags: str):
        if config_fname is None:
            return None

        f = open(config_fname)
        content = f.read()
        f.close()

        if cls._findbool('BR2_PACKAGE_RTSKEY=y', content) is False:
            return None

        cls.rtskey_out_dir = cls._findstring('BR2_RTSKEY_OUT_DIR=.*', content)
        if "$(BASE_DIR)" in cls.rtskey_out_dir:
            base_dir = os.path.dirname(config_fname)
            cls.rtskey_out_dir = cls.rtskey_out_dir.replace("$(BASE_DIR)", base_dir)

        cls.rtskey_target_dir = cls._findstring('BR2_RTSKEY_TARGET_DIR=.*', content)

        if stype == "crypto":
            if group == "user" and \
                not cls._findbool('BR2_CONFIG_CRYPTO_BOOT_USER_PARTITION=y', \
                                  content):
                return None

            if cls._findbool('BR2_CONFIG_FULL_DISK_ENCRYPTION=y', content):
                return fde(content)

            if cls._findbool('BR2_CONFIG_FILE_BASED_ENCRYPTION=y', content):
                return fbe(content)

        elif stype == "secure":
            if group == "user" and \
                not cls._findbool('BR2_CONFIG_SECURE_BOOT_USER_PARTITION=y', \
                                  content):
                return None

            if cls._findbool('BR2_CONFIG_SECURE_DM_MAPPER=y', content):
                if "ro" in flags:
                    return dmverity(content)
                else:
                    return dmintegrity()

            if cls._findbool('BR2_CONFIG_UBIFS_FS_AUTHENTICATION=y', content):
                return ubifs_auth(content)

        return None

    @staticmethod
    def _findstring(pattern: str, content: str) -> str:
        tmp = re.findall(pattern, content)
        if len(tmp) == 1:
            return tmp[0].split("=")[1].split('"')[1]
        return ""

    @staticmethod
    def _findbool(pattern: str, content: str) -> bool:
        if len(re.findall(pattern, content)) == 1:
            return True
        return False

    @abstractmethod
    def get_key_path(self) -> str:
        pass

    @abstractmethod
    def get_target_key_path(self) -> str:
        pass

    def get_cipher(self) -> str:
        return ""

    def get_cert_path(self) -> str:
        return ""

    def get_mount_flags(self) -> str:
        return ""

    def get_target_name(self, mnt_point: str) -> str:
        return ""

    def get_target_dev(self, mnt_point: str) -> str:
        return ""

    def get_dm_table(self, info: str, size: str, mnt_point: str, dev: str) -> str:
        return ""


class fde (Secure_mode):
    name: str = "dm-crypt"
    target_type: str = "crypt"


    def __init__(self, content: str):
        self.crypto_key = self._findstring('BR2_CRYPTO_BOOT_KEY=.*', content)

    def get_key_path(self) -> str:
        return self.rtskey_out_dir + "/" + self.crypto_key + ".bin"

    def get_target_key_path(self) -> str:
        return self.rtskey_target_dir + "/" + self.crypto_key + ".bin"

    def get_target_name(self, mnt_point: str) -> str:
        mnt = os.path.basename(mnt_point)
        return fde.name + "-" + mnt

    def get_target_dev(self, mnt_point: str) -> str:
        mnt = os.path.basename(mnt_point)
        return "/dev/mapper/" + fde.name + "-" + mnt

    def get_dm_table(self, info: str, size: str, mnt_point: str, dev: str) -> str:
        start_sector: int = 0
        dev_sector_size: int = int(size / 512)
        cipher_mode_iv: str = "aes-cbc-plain64"
        iv_offset : int = 0
        encrypt_offset: int = 0
        optional_params = ["sector_size:4096", "iv_large_sectors"]
        otp_key: str = '0' * 2 * os.path.getsize(self.get_key_path())

        table =  " ".join((str(start_sector), str(dev_sector_size),
                         fde.target_type, cipher_mode_iv, otp_key,
                         str(iv_offset), dev, str(encrypt_offset),
                         str(len(optional_params))
                         ))
        return table + " " + " ".join(optional_params)


class fbe (Secure_mode):
    name: str = "fbe"

    def __init__(self, content: str):
        self.fbe_key = self._findstring('BR2_CRYPTO_FBE_KEY=.*', content)
        if self._findbool('BR2_CONFIG_FBE_AES_256_XTS=y', content):
            self.cipher = 'AES-256-XTS'
        else:
            self.cipher = 'AES-128-CBC'

    def get_key_path(self) -> str:
        return self.rtskey_out_dir + "/" + self.fbe_key + ".bin"

    def get_target_key_path(self) -> str:
        return self.rtskey_target_dir + "/" + self.fbe_key + ".bin"

    def get_cipher(self) -> str:
        return self.cipher


class dmverity (Secure_mode):
    name: str = "dm-verity"
    target_type: str = "verity"

    def __init__(self, content: str):
        self.secure_data_key = self._findstring('BR2_SECURE_DATA_KEY=.*',
                                                content)

    def get_key_path(self) -> str:
        return self.rtskey_out_dir + "/" + self.secure_data_key + ".key"

    def get_target_key_path(self) -> str:
        return self.rtskey_target_dir + "/" + self.secure_data_key + ".der"

    def get_target_name(self, mnt_point: str) -> str:
        mnt = os.path.basename(mnt_point)
        return dmverity.name + "-" + mnt

    def get_target_dev(self, mnt_point: str) -> str:
        mnt = os.path.basename(mnt_point)
        return "/dev/mapper/" + dmverity.name + "-" + mnt

    def get_dm_table(self, info: str, size: str, mnt_point: str, dev: str) -> str:
        return build_verity_img.build_verity_table(info, mnt_point, dev)\
            .split("\"")[1]


class dmintegrity (Secure_mode):
    name: str = "dm-integrity"
    target_type: str = "integrity"

    def get_key_path(self) -> str:
        return ""

    def get_target_key_path(self) -> str:
        return ""

    def get_target_name(self, mnt_point: str) -> str:
        mnt = os.path.basename(mnt_point)
        return dmintegrity.name + "-" + mnt

    def get_target_dev(self, mnt_point: str) -> str:
        mnt = os.path.basename(mnt_point)
        return "/dev/mapper/" + dmintegrity.name + "-" + mnt

    def get_dmintegrity_type(self, config: str) -> int:
        self.pack_dmintegrity: int = 0
        f = open(config)
        content = f.read()
        f.close()

        if (len(re.findall('BR2_CONFIG_SECURE_BOOT_PACK_DMINTEGRITY=y', content)) == 1):
            self.pack_dmintegrity = 1
        return self.pack_dmintegrity

class ubifs_auth (Secure_mode):
    name: str = "ubifs_auth"

    def __init__(self, content: str):
        self.secure_data_key = self._findstring('BR2_SECURE_DATA_KEY=.*',
                                                content)
        self.ubifs_auth_key = self._findstring('BR2_SECURE_UBIFS_AUTH_KEY=.*',
                                               content)

    def get_key_path(self) -> str:
        return self.rtskey_out_dir + "/" + self.secure_data_key + ".key"

    def get_target_key_path(self) -> str:
        return self.rtskey_target_dir + "/" + self.ubifs_auth_key + ".bin"

    def get_cert_path(self) -> str:
        return self.rtskey_out_dir + "/" + self.secure_data_key + ".crt"

    def get_mount_flags(self) -> str:
        return "auth_key=ubifs:foo,auth_hash_name=sha256"


class Fstab ():

    @classmethod
    def get_head_line(cls):
        return "#<type>\t<src>\t<mnt_point>\t<fstype>\t<mnt_flags and options>\t<fsmgr_flags>"

    def __init__(self, type: str, node: Node):
        self.type = type
        self.node = node
        self.__parse_node()

    def __parse_node(self):
        self.group = self.node.get_field("group")
        self.dev = self.node.get_field("dev")
        self.mnt_point = self.node.get_field("mnt_point")
        self.fstype = self.node.get_field("fstype")
        self.mnt_flags = self.node.get_field("mnt_flags")
        self.fsmgr_flags = self.node.get_field("fsmgr_flags")

    def is_support_crypto(self):
        if "encrypt" in self.fsmgr_flags:
            return True
        return False

    def is_support_secure(self):
        if "verify" in self.fsmgr_flags:
            return True
        return False

    def update_fsmgr_flags(self, cmode: Secure_mode, smode: Secure_mode):
        flags = self.fsmgr_flags.split(',')

        if "encrypt" in flags:
            i = flags.index("encrypt")
            if cmode == None:
                flags.pop(i)
            elif isinstance(cmode, fde):
                flags[i] = "encrypt=" + cmode.get_target_key_path()
            elif isinstance(cmode, fbe):
                if self.group == "root":
                    flags[i] = "encrypt=" + cmode.get_target_key_path()
                flags.append("fscrypt")

        if "verify" in flags:
            i = flags.index("verify")
            if smode == None:
                flags.pop(i)
            elif isinstance(smode, dmverity):
                flags[i] = "verify=" + smode.get_target_key_path()
            elif isinstance(smode, dmintegrity):
                flags.append("dm-integrity")
            elif isinstance(smode, ubifs_auth):
                if self.group == "root":
                    flags[i] = "verify=" + smode.get_target_key_path()
                self.mnt_flags += "," + smode.get_mount_flags()
                flags.append("ubifs_auth")

        if not flags:
            self.fsmgr_flags = "defaults"
        else:
            self.fsmgr_flags = ",".join(flags)

        return 0

    def get_line(self):
        return "\t".join((self.type, self.dev, self.mnt_point, self.fstype, \
                          self.mnt_flags, self.fsmgr_flags))

    def get_group(self):
        return self.group

    def get_dev(self):
        return self.dev

    def get_mnt_point(self):
        return self.mnt_point

    def get_fstype(self):
        return self.fstype

    def get_mnt_flags(self):
        return self.mnt_flags


class Part ():
    def __init__(self, node: Node, sector_size: int, flash_type: str, \
                 config_fname: str, index: int):
        self.node = node
        self.index = index
        self.sectors = 0
        self.readonly = 0
        self.sector_size = sector_size
        self.flash_type = flash_type
        self.fstab = None
        self.cmode: Secure_mode = None
        self.smode: Secure_mode = None
        self.__parse_node()
        self.__parse_security_mode(config_fname)

    def __parse_node(self):
        self.name = self.node.get_field("label")
        self.addr = self.node.address
        reg = self.node.get_reg()
        self.start = reg.values[0]
        self.size = reg.values[1]
        self.sectors =  int(self.size/self.sector_size)

        nodes = self.node.child_nodes()
        while True:
            try:
                n = next(nodes)
                if n.name == "fstab":
                    self.fstab = Fstab(self.flash_type, n)
            except StopIteration:
                break

    def __parse_security_mode(self, config_fname: str):
        if self.fstab is not None:
            group = self.fstab.get_group()
            flags = self.fstab.get_mnt_flags()

            if self.fstab.is_support_crypto():
                self.cmode = Secure_mode.create_object(config_fname, "crypto", \
                                        group, flags)

            if self.fstab.is_support_secure():
                self.smode = Secure_mode.create_object(config_fname, "secure", \
                                        group, flags)

            self.fstab.update_fsmgr_flags(self.cmode, self.smode)
        return 0

    def get_size(self):
        return self.size

    def get_sectors(self):
        return self.sectors

    def dump(self):
        print(self.name, self.start, self.size, self.readonly, self.flash_type)

    def injection_error(self):
        self.sectors = -1

    def get_group(self) -> str:
        if self.fstab is None:
            return ""

        return self.fstab.get_group()

    def get_dev(self) -> str:
        if self.fstab is None:
            return ""

        return self.fstab.get_dev()

    def get_mnt_point(self) -> str:
        if self.fstab is None:
            return ""

        return self.fstab.get_mnt_point()

    def get_security_mode(self):
        return self.cmode, self.smode

    def generate_dm_init_table(self, info: str, ofname: str):
        table = r'dm-mod.create=\"'
        id = 0
        dev = self.get_dev()
        mnt = self.get_mnt_point()

        if self.cmode is not None:
            name = self.cmode.get_target_name(mnt)
            table +=  name + ",,,ro, " + \
                self.cmode.get_dm_table(info, self.size, mnt, dev)

        if self.smode is not None:
            if self.cmode is not None:
                table += ";"
                dev = "/dev/dm-%d" %(id)
                id = 1

            name = self.smode.get_target_name(mnt)
            table +=  name + ",,,ro, " + \
                self.smode.get_dm_table(info, self.size, mnt, dev)

        table += r'\" root=/dev/dm-%d' %(id)

        with open(ofname, "w") as f:
            f.write(table)

        return None

    def get_dmverity_data_dev(self) -> str:
        if not isinstance(self.smode, dmverity):
            return ""

        if isinstance(self.cmode, fde):
            return self.cmode.get_target_dev(self.get_mnt_point())
        else:
            return self.get_dev()


class Parts():
    def __init__(self, dtsfname: str, config_fname: str):
        self.flash_size = 0x1000000
        self.sector_size = 0x10000
        self.flash_type = "nor"
        self.parts = []
        self.__parse_dts(dtsfname, config_fname)
        if self.__update() < 0:
            self.__injection_error()
        self.__dump()

    def __parse_dts(self, dtsfname: str, config_fname: str):
        tree = Devicetree.parseFile(dtsfname, False)
        self.node = tree.match("fixed-partitions")[0]
        nodes = self.node.child_nodes()
        index = 0
        while True:
            try:
                n = next(nodes)
                name = n.get_field("label")
                reg = n.get_reg()

                if n.name == 'global':
                    print("parse node: %s" % name)
                    self.flash_size = reg.values[1]
                    self.flash_type = n.get_field("type")
                    self.sector_size = n.get_field("sector_size")

                elif n.name == "partition":
                    print("parse node: %s" % name)
                    index = index + 1
                    self.parts.append(Part(n, self.sector_size, self.flash_type, \
                                           config_fname, index))
            except StopIteration:
                break

    def to_dts(self, fname: str):
        with open(fname, "w") as f:
            f.write(self.node.to_dts())
        return None

    def creat_gpt(self, dir: str):
        root_directory = dir
        host_parted = root_directory + "/host/sbin/parted"
        gpt_file = root_directory + "/images/gpt"
        if os.path.exists(gpt_file):
            os.remove(gpt_file)
        flash_sectors = self.flash_size / self.sector_size
        cmd = "dd if=/dev/zero of=%s bs=%d count=0 seek=%d" % (gpt_file, self.sector_size, flash_sectors)
        print(cmd)
        os.system(cmd)
        cmd = "%s %s mklabel gpt" % (host_parted, gpt_file)
        print(cmd)
        os.system(cmd)

    def create_gpt_partition(self, dir: str):
        root_directory = dir
        host_parted = root_directory + "/host/sbin/parted"
        gpt_file = root_directory + "/images/gpt"
        gpt_file_head = root_directory + "/images/gpt_head"
        gpt_file_tail = root_directory + "/images/gpt_tail"

        index = 1
        if os.path.exists(gpt_file):
            for p in self.parts:
                if p.flash_type == "emmc" and p.name != "boot" and p.name != "dtb":
                    end = p.start + p.size - 1
                    cmd = "%s %s mkpart primary %dB %dB" % (host_parted, gpt_file, p.start, end)
                    print(cmd)
                    os.system(cmd)
                    cmd = "%s %s name %d %s" % (host_parted, gpt_file, index, p.name)
                    print(cmd)
                    os.system(cmd)
                    index = index + 1
            cmd = "dd if=%s of=%s bs=512 count=34" % (gpt_file, gpt_file_head)
            print(cmd)
            os.system(cmd)
            cmd = "dd if=%s of=%s bs=512 count=33 skip=%d" % (gpt_file, gpt_file_tail, self.flash_size / 512 -33)
            print(cmd)
            os.system(cmd)
            os.remove(gpt_file)

    def __injection_error(self):
        for p in self.parts:
            p.injection_error()


    def __dump(self):
        for p in self.parts:
            p.dump()

    def __update(self):
        offset = 0
        for p in self.parts:
            if p.start == 0:
                offset = p.size
            else:
                if p.start < offset:
                    print("Error: offset cross", p.dump())
                    return -1
                offset = p.start + p.size
        if offset > self.flash_size:
            print("offset", offset, self.flash_size)
            print("Error: exceed the %s size" % self.flash_type)
        return 0

    def get_burn_header(self, name: str):
        for p in self.parts:
            if p.name == name:
                return (p.start, p.sectors, p.sector_size, p.flash_type)
        return (0, 0, 0, 0)

    def generate_fstab(self, group: str, ofname: str):
        with open(ofname, "w") as f:
            f.write(Fstab.get_head_line() + "\n")

            for p in self.parts:
                if group == p.get_group():
                    f.write(p.fstab.get_line() + "\n")

    def get_part(self, name: str):
        for p in self.parts:
            if p.name == name:
                return p
        return None

    def rauconf(self, fname: str):
        config = configparser.ConfigParser()
        config.add_section("system")
        config.set("system", "compatible", "Realtek")
        config.set("system", "bootloader", "uboot")
        config.set("system", "statusfile", "/tmp/central-status.raucs")
        config.add_section("keyring")
        config.set("keyring", "path", "trunk.cert.pem")
        for p in self.parts:
            if (p.name.endswith("_b") == False):
                section = "slot." + p.name + ".0"
                config.add_section(section)
                config.set(section, 'device', "/dev/mtd" + str(p.index))
                config.set(section, 'type', p.flash_type)
                if p.name == "kernel":
                    config.set(section, 'bootname', "A")
                else:
                    config.set(section, 'parent', "kernel.0")
                config.set(section, 'install-same', "false")
        for p in self.parts:
            if (p.name.endswith("_b") == False):
                section = "slot." + p.name + ".1"
                if config.has_section(section):
                    continue
            else:
                section = "slot." + p.name[:-2] + ".1"
            if not config.has_section(section):
                config.add_section(section)
            config.set(section, 'device', "/dev/mtd" + str(p.index))
            config.set(section, 'type', p.flash_type)
            if p.name.find("kernel") == 0:
                config.set(section, 'bootname', "B")
            else:
                config.set(section, 'parent', "kernel.1")
            config.set(section, 'install-same', "false")
        fd = open(fname, 'w')
        config.write(fd)
        fd.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    parset_fstab = subparsers.add_parser('fstab')
    parset_fstab.add_argument('group', action='store', help="fstab group: 'root' or 'user'")
    parset_fstab.add_argument('config', action='store', help='input board config file name')
    parset_fstab.add_argument('ifname', action='store', help='input dts file name')
    parset_fstab.add_argument('ofname', action='store', help='output fstab file name')
    parset_fstab.set_defaults(dest='fstab')

    parset_todts = subparsers.add_parser('todts')
    parset_todts.add_argument('ifname', action='store', help='input dts file name')
    parset_todts.add_argument('ofname', action='store', help='output dts file name')
    parset_todts.set_defaults(dest='todts')

    parset_dm_init = subparsers.add_parser('dminit')
    parset_dm_init.add_argument('config', action='store', help='input board config file name')
    parset_dm_init.add_argument('ifname', action='store', help='input dts file name')
    parset_dm_init.add_argument('info', action='store', help='dm-verity info')
    parset_dm_init.add_argument('ofname', action='store', help='output dminit file name')
    parset_dm_init.set_defaults(dest='dminit')

    parset_rauconf = subparsers.add_parser('rauconf')
    parset_rauconf.add_argument('ifname', action='store', help='input dts file name')
    parset_rauconf.add_argument('ofname', action='store', help='out put rauc conf file name')
    parset_rauconf.set_defaults(dest='rauconf')

    args = parser.parse_args()

    if args.dest == 'fstab':
        Parts(args.ifname, args.config).generate_fstab(args.group, args.ofname)
    if args.dest == 'dminit':
        Parts(args.ifname, args.config).get_part("rootfs").generate_dm_init_table(args.info, args.ofname)
    if args.dest == 'todts':
        Parts(args.ifname, None).to_dts(args.ofname)
    if args.dest == 'rauconf':
        Parts(args.ifname, None).rauconf(args.ofname)
