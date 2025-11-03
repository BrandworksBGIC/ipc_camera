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
import hashlib

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

def create_generate_file(fname):
    with open(fname, "w") as f:
        f.write("#!/bin/bash\n")
        f.write("\n")
        f.write("FILES=\"sw-description sw-description.sig rootfs.img kernel.img info.json\"\n")
        f.write("for i in $FILES;do\n")
        f.write("        echo $i;done | $1 -ov -H crc >  $2\n")
    f.close()

def encrypt_file(filename):
    with open(filename, 'rb') as file:
        content = file.read()
    # 创建SHA-256对象并计算文件内容的哈希值
    hasher = hashlib.sha256()
    hasher.update(content)
    encrypted_data = hasher.hexdigest()

    return encrypted_data

def write_part_in_file(f, part, images_ini, swu_dir):
        f.write("      \"%s\": {\n" % (part))
        f.write("        \"images\": [\n")
        for key, fname in images_ini:
            if key == "kernel":
                name = key + ".img"
                #print("name:%s" % (name))
                f.write("          {\n")
                f.write("            \"filename\": \"%s\",\n" % (name))
                if part == "part_A":
                    f.write("            \"device\": \"/dev/mtd2\",\n")
                if part == "part_B":
                    f.write("            \"device\": \"/dev/mtd4\",\n")
                f.write("            \"type\": \"flash\",\n")
                cmd = "%s/%s" % (swu_dir, name)
                #print("%s" % (cmd))
                sha256_result = encrypt_file(cmd)
                #print("%s" % (sha256_result))
                f.write("            \"sha256\": \"%s\"\n" % (sha256_result))
                f.write("          },\n")
            if key == "rootfs":
                name = key + ".img"
                #print("name:%s" % (name))
                f.write("          {\n")
                f.write("            \"filename\": \"%s\",\n" % (name))
                if part == "part_A":
                    f.write("            \"device\": \"/dev/mtd3\",\n")
                if part == "part_B":
                    f.write("            \"device\": \"/dev/mtd5\",\n")
                f.write("            \"type\": \"flash\",\n")
                #f.write("            \"type\": \"raw\",\n")
                cmd = "%s/%s" % (swu_dir, name)
                #print("%s" % (cmd))
                sha256_result = encrypt_file(cmd)
                #print("%s" % (sha256_result))
                f.write("            \"sha256\": \"%s\",\n" % (sha256_result))
                #f.write("            \"installed-directly\": \"true\"\n")
                f.write("          }\n")
        f.write("      ]\n")
        if part == "part_A":
                f.write("      },\n")
        if part == "part_B":
                f.write("      }\n")

def create_file(fname, images_ini, swu_dir):
    with open(fname, "w") as f:
        f.write("{\n")
        f.write("  \"software\": {\n")
        f.write("    \"stable\": {\n")

        write_part_in_file(f, "part_A", images_ini, swu_dir)
        write_part_in_file(f, "part_B", images_ini, swu_dir)

        f.write("    },\n")
        f.write("    \"version\": \"1.0.1\",\n")
        f.write("    \"hardware-compatibility\": [\n")
        f.write("      \"1.0\"\n")
        f.write("    ]\n")
        f.write("  }\n")
        f.write("}\n")
    f.close()

def copy_file(src_path, dst_path, key, file_name):
    if not os.path.exists(src_path):
        return False
    full_path = os.path.join(src_path, file_name)
    if not os.path.exists(full_path):
        return False
    print(full_path)
    print(file_name)

    shutil.copy(full_path, dst_path + "/" + key + ".img")
    return True

if __name__ == "__main__":
    scripts_directory,scripts_name = os.path.split(sys.argv[0])
    output_dir = os.path.normpath(sys.argv[1])
    user_fs = os.path.normpath(sys.argv[2])
    board_config = os.path.normpath(sys.argv[3])
    images_dir = output_dir + "/images"
    swu_dir = output_dir + "/swu"
    host_cpio = output_dir + "/host/bin/cpio"
    host_openssl = output_dir + "/host/bin/openssl"
    key_dir = output_dir + "/build/ota_upfw/"
    swu_des_file = swu_dir + "/sw-description"
    swu_gen_file = swu_dir + "/generate_swu.sh"
    swu_file = output_dir + "/images" + "/linux.swu"
    build_dir = output_dir + "/build"
    linux_dir = build_dir + "/linux-custom"
    merge_ini = output_dir + "/merge.ini"
    info_file = swu_dir + "/info.json"

    print("board_config", board_config)
    gl = globalval.globalval(board_config)
    ic_type = gl.get_ic_type()
    default_config = gl.get_defconfig(linux_dir)

    ps = part.Parts(default_config, board_config)

    config = merge.configure(ps, output_dir)
    images_ini = merge.file_pairs(output_dir, scripts_directory, merge_ini, user_fs, ps)

    mk_dir(swu_dir)

    for key, fname in images_ini:
        fname = os.path.basename(fname)
        if key == "kernel":
            copy_file(images_dir, swu_dir, key, fname)
        if key == "rootfs":
            fname="rootfs.ubi"
            copy_file(images_dir, swu_dir, key, fname)

    ver_info = burnheader.Header_info(output_dir, scripts_directory, user_fs, images_ini, config)
    create_info_file(info_file, ver_info, config)

    create_file(swu_des_file, images_ini, swu_dir)
    create_generate_file(swu_gen_file)

    if os.path.exists(swu_file):
        cmd = "rm %s" % (swu_file)
        send_cmd(cmd)

    os.chmod(key_dir, stat.S_IRWXU)
    os.chmod(swu_dir, stat.S_IRWXU)
    end_cmd = "-outform DER -nosmimecap -binary"
    cmd = "%s cms -sign -in %s -out %s.sig -signer %strunk.cert.pem -inkey %strunk.key.pem %s" % (host_openssl, swu_des_file, swu_des_file, key_dir, key_dir, end_cmd)
    send_cmd(cmd)

    cmd = "cd %s;sh generate_swu.sh %s %s" % (swu_dir, host_cpio, swu_file)
    send_cmd(cmd)

    cmd = "rm -rf %s" % (swu_dir)
    send_cmd(cmd)
    fd = open(swu_file, "a")
    fd.write('FEOF')
    fd.close()
