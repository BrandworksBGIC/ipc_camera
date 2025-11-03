#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import sys
import stat
import shutil
import re
import struct
import binascii
import datetime
import globalval
import burnheader
import part
import merge

def send_cmd(cmd):
    print(cmd)
    os.system(cmd)

def mk_dir(path):
    if not os.path.exists(path):
        os.makedirs(path)
        print("create %s" % (path))

def create_info_file(fname, ver_info, config):
    with open(fname, "w") as f:
        f.write("Kernel Version:%s\n" % (ver_info.kernel_version()))
        f.write("DDR Type:%x\n" % (ver_info.parse_ddr_type()))
        f.write("IC Version:%s\n" % (ver_info.parse_ic_type()))
        f.write("Sdk Version:%s\n" % (ver_info.sdk_version()))
        f.write("Signature:%s\n" % (ver_info.signature()))
        f.write("Build Time:%s%s\n" % (ver_info.build_date(), ver_info.build_time()))
        f.write("Kernel Time:%s\n" % (config.get_date_time("kernel")))
        f.write("Rootfs Time:%s\n" % (config.get_date_time("rootfs")))

def create_file(fname, images_ini):
    line = datetime.datetime.now().strftime("%Y%m%d")
    with open(fname, "w") as f:
        f.write("[update]\n")
        f.write("compatible=Realtek\n")
        f.write("version=%s\n" % (line))
        f.write("\n")
        for key, fname in images_ini:
            name = "[image." + key + "]\n"
            f.write(name)
            name = "filename=" + key + ".img\n"
            f.write(name)
    f.close()

def copy_file(src_path, dst_path, key, file_name):
    if not os.path.exists(src_path):
        return False
    full_path = os.path.join(src_path, file_name)
    if not os.path.exists(full_path):
        return False
    print(full_path)
    shutil.copy(full_path, dst_path + "/" + key + ".img")
    return True

if __name__ == "__main__":
    scripts_directory,scripts_name = os.path.split(sys.argv[0])
    output_dir = os.path.normpath(sys.argv[1])
    user_fs = os.path.normpath(sys.argv[2])
    board_config = os.path.normpath(sys.argv[3])
    images_dir = output_dir + "/images"
    raucb_dir = output_dir + "/raucb"
    host_rauc = output_dir + "/host/bin/rauc"
    key_dir = output_dir + "/build/ota_upfw/"
    raucm_file = raucb_dir + "/manifest.raucm"
    raucb_file = output_dir + "/images" + "/linux.raucb"
    build_dir = output_dir + "/build"
    uboot_dir = build_dir + "/uboot-custom"
    linux_dir = build_dir + "/linux-custom"
    merge_ini = output_dir + "/merge.ini"
    info_file = raucb_dir + "/info.json"
    dtb_kernel_rootfs = 0

    print("board_config", board_config)
    gl = globalval.globalval(board_config)
    ic_type = gl.get_ic_type()
    default_config = gl.get_defconfig(linux_dir)

    ps = part.Parts(default_config, board_config)

    config = merge.configure(ps, output_dir)
    images_ini = merge.file_pairs(output_dir, scripts_directory, merge_ini, user_fs, ps)

    mk_dir(raucb_dir)
    create_file(raucm_file, images_ini)

    for key, fname in images_ini:
        fname = os.path.basename(fname)
        if key == "dtb":
            dtb_kernel_rootfs = dtb_kernel_rootfs | 1
        if key == "kernel":
            dtb_kernel_rootfs = dtb_kernel_rootfs | 2
        if key == "rootfs":
            dtb_kernel_rootfs = dtb_kernel_rootfs | 4
        if not copy_file(images_dir, raucb_dir, key, fname):
            print("Error, not found file:", fname)
    if dtb_kernel_rootfs != 7:
        print("Warning: dtb, kernel, rootfs should be updated together!")

    ver_info = burnheader.Header_info(output_dir, scripts_directory, user_fs, images_ini, config)
    create_info_file(info_file, ver_info, config)

    if os.path.exists(raucb_file):
        cmd = "rm %s" % (raucb_file)
        send_cmd(cmd)
    os.chmod(key_dir, stat.S_IRWXU)
    cmd = "%s --cert=%strunk.cert.pem --key=%strunk.key.pem bundle %s %s" % (host_rauc, key_dir, key_dir, raucb_dir, raucb_file)
    send_cmd(cmd)
    cmd = "rm -rf %s" % (raucb_dir)
    send_cmd(cmd)
    fd = open(raucb_file, "a")
    fd.write('FEOF')
    fd.close()
