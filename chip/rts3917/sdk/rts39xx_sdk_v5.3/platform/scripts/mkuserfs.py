#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import math
import part
from part import fbe, ubifs_auth

def send_cmd(cmd):
    print(cmd)
    os.system(cmd)

def pathj(dname, bname):
    return os.path.join(dname, bname)

def create_squashfs(host_mksquashfs, dname, size, dst_name):
    cmd = "%s %s %s -comp xz" % (host_mksquashfs, dname, dst_name)
    send_cmd(cmd)

def create_jffs2(host_mkjffs2, dname, size, dst_name):
    fname = pathj(dname, ".fs_state")
    if not os.path.exists(fname):
        cmd = "ln -sf 2 %s" % (fname)
        send_cmd(cmd)
    cmd = "%s -r %s -e 0x10000 --pad=0x%x -l -o %s" % (host_mkjffs2, dname, size, dst_name)
    send_cmd(cmd)

def create_ubi(host_ubi_path, dname, sectors, ubinize_cfg, dst_ubifs, dst_name, cmode, smode):
    fname = pathj(dname, ".fs_state")
    if not os.path.exists(fname):
        cmd = "ln -sf 2 %s" % (fname)
        send_cmd(cmd)

    host_mkubifs = host_ubi_path + "mkfs.ubifs"
    host_mkubi = host_ubi_path + "ubinize"
    cmd = "%s -d %s -e 0x1f000 -c %d -m 0x800 -x lzo -F" \
        % (host_mkubifs, dname, sectors)

    if isinstance(cmode, fbe):
        cmd += " --cipher %s --key %s" \
            %(cmode.get_cipher(), cmode.get_key_path())

    if isinstance(smode, ubifs_auth):
        cmd += " --hash-algo sha256 --auth-key %s --auth-cert %s" \
            %(smode.get_key_path(), smode.get_cert_path())

    cmd += " -o %s" % (dst_ubifs)
    send_cmd(cmd)

    cmd = "%s -m 0x800 -p 0x20000 -s 2048 %s -o %s" % (host_mkubi, ubinize_cfg, dst_name)
    send_cmd(cmd)

def getdirsize(dir):
   size = 0
   for root, dirs, files in os.walk(dir):
       size += sum([os.path.getsize(os.path.join(root, name)) for name in files])
   return size

def create_ext4(host_mkext4_path, dname, size, dst_name, p):
    host_mkext4 = host_mkext4_path + "mkfs.ext4"
    mount_flag = p.fstab.get_mnt_flags()
    size = size /1024 / 1024
    if not os.path.exists(dname):
        os.mkdir(dname)
    sz = getdirsize(dname)
    sz = sz / 1024 / 1024
    if sz <= 10:
        if "ro" in mount_flag:
            sz = sz + 2
        else:
            sz = sz + 8
    else:
        if "ro" in mount_flag:
            sz = sz * 1.2
        else:
            sz = sz * 1.2 + 4
    sz = math.ceil(sz)
    if (sz > size):
        sz = size

    if os.path.exists(dst_name):
        os.remove(dst_name)

    cmd = "%s -b 4096 -d %s %s  \"%dM\"" % (host_mkext4, dname, dst_name, sz)
    send_cmd(cmd)

def create(output_dir, scripts_directory, key, user_fs, p):
    images_dir = output_dir + "/images"
    host_mkjffs2 = output_dir + "/host/sbin/mkfs.jffs2"
    host_mksquashfs = output_dir + "/host/bin/mksquashfs"
    host_mkubi_path = output_dir + "/host/sbin/"
    host_mkext4_path = output_dir + "/host/sbin/"
    ubinize_cfg = output_dir + "/user_ubinize.cfg"
    ubinize_cfg_template = scripts_directory + "/ubinize.cfg"
    dst_userfs_img = images_dir + "/" + key + ".bin"
    dst_ubifs = images_dir + "/" + key + ".ubifs"
    eb_cnt = p.get_sectors()
    size = p.get_size()

    dname = output_dir + "/" + key
    if not os.path.exists(dname):
        os.mkdir(dname)

    if user_fs == "squashfs":
        create_squashfs(host_mksquashfs, dname, size, dst_userfs_img)

    if user_fs == "jffs2":
        create_jffs2(host_mkjffs2, dname, size, dst_userfs_img)

    if user_fs == "ubi":
        cmd = "sed 's;BR2_USER_DATA_UBIFS_PATH;%s;' %s > %s" % (dst_ubifs, ubinize_cfg_template, ubinize_cfg)
        send_cmd(cmd)
        cmode, smode = p.get_security_mode()
        create_ubi(host_mkubi_path, dname, eb_cnt, ubinize_cfg, dst_ubifs, dst_userfs_img, cmode, smode)
        cmd = "rm %s" % (ubinize_cfg)
        send_cmd(cmd)

    if user_fs == "ext4":
        create_ext4(host_mkext4_path, dname, size, dst_userfs_img, p)
