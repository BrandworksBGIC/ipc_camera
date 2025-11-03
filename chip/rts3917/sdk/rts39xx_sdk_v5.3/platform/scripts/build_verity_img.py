#! /usr/bin/env python3
#
# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import sys
import struct
import tempfile
import subprocess
import binascii
from Cryptodome.Hash import SHA256
from Cryptodome.PublicKey import RSA
from Cryptodome.Signature import PKCS1_v1_5

VERSION = 0
MAGIC_NUMBER = 0xb001b001
BLOCK_SIZE = 4096
METADATA_BLOCKS = 8
METADATA_SIZE = BLOCK_SIZE * METADATA_BLOCKS

def run(cmd):
    ret = subprocess.call(cmd, shell=True)
    if ret:
        exit(-1)

def get_verity_metadata_size(data_size):
    return METADATA_SIZE

def build_metadata_block(verity_table, signature):
    table_len = len(verity_table)
    block = struct.pack("II256sI", MAGIC_NUMBER, VERSION, signature, table_len)
    block += verity_table.encode()
    block = block.ljust(METADATA_SIZE, b'\x00')
    return block

def sign_verity_table(table, key_path):
    key = RSA.importKey(open(key_path).read())
    sign = PKCS1_v1_5.new(key)

    sha_value = SHA256.new(table.encode())
    sign_value = sign.sign(sha_value)

    print("sign verity table:")
    print("sha256: " +  sha_value.hexdigest())
    print("sign value: " + sign_value.hex())
    return sign_value

def build_verity_table(info, mount_point, block_device):
    mount_path,mount_name = os.path.split(mount_point)

    with open(info) as files:
        for line in files:
            if "Root hash" in line:
                root_hash = line.partition(":")[2].strip()
            elif "Salt" in line:
                salt = line.partition(":")[2].strip()
            elif "Data blocks" in line:
                data_blocks = line.partition(":")[2].strip()
                hash_offset = str(int(data_blocks) + METADATA_BLOCKS + 1)

    table = " ".join(("dmsetup create",
                    "dm-verity-" + mount_name,
                    "-r --table \"0",
                    str(int(data_blocks) * 8),
                    "verity 1",
                    block_device, block_device,
                    str(BLOCK_SIZE), str(BLOCK_SIZE),
                    data_blocks, hash_offset,
                    "sha256", root_hash, salt + "\""
                    ))

    print("verity table: " + table)
    return table

def build_verity_tree(tool, partition):
    part_path,part_name = os.path.split(partition)
    tree = partition + ".tree"
    info = partition + ".verity.info"

    size = os.path.getsize(partition)
    cmd = " ".join((tool + "/veritysetup", "format",
                    "--hash sha256",
                    "--data-block-size", str(BLOCK_SIZE),
                    "--hash-block-size", str(BLOCK_SIZE),
                    partition, tree, ">", info))
    print(cmd)
    run(cmd);

    return info, tree

def build_verity_metadata(verity_table, mount_point, signing_key, metadata_image):
    # build the verity table signature
    signature = sign_verity_table(verity_table, signing_key)
    # build the metadata block
    metadata_block = build_metadata_block(verity_table, signature)
    # write it to the outfile
    with open(metadata_image, "ab") as f:
        f.write(metadata_block)
    return

def build_verity_img(tool, partition, mount_point, block_device, signing_key, image):
    path,name = os.path.split(partition)
    print( "\033[1;47;30m" + "build verity img: " + name + "\033[0m")

    cmd = " ".join(("cp -f", partition, image))
    print(cmd)
    run(cmd);

    print("\nbuild verity tree:")
    verity_info, verity_tree = build_verity_tree(tool, partition)
    print("\nbuild verity table:")
    verity_table = build_verity_table(verity_info, mount_point, block_device)
    print("\nbuild verity metadata:")
    build_verity_metadata(verity_table, mount_point, signing_key, image)

    cmd = " ".join(("cat ", verity_tree, ">>", image))
    print(cmd)
    run(cmd);

    print( "\033[1;47;30m" + "build successfully!" +  "\033[0m")
    return


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    parser_size = subparsers.add_parser('size')
    parser_size.add_argument('partition_size', type=int, action='store', help='partition size')
    parser_size.set_defaults(dest='size')

    parset_build = subparsers.add_parser('build')
    parset_build.add_argument('tool', action='store', help='tool path')
    parset_build.add_argument('partition', action='store', help='partiton path')
    parset_build.add_argument('mount_point', action='store', help='mount path')
    parset_build.add_argument('block_device', action='store', help='block device')
    parset_build.add_argument('signing_key', action='store', help='signing key path')
    parset_build.add_argument('image', action='store', help='out image path')
    parset_build.set_defaults(dest='build')

    args = parser.parse_args()

    if args.dest == 'size':
        print(get_verity_metadata_size(args.partition_size))
    else:
        build_verity_img(args.tool, args.partition, args.mount_point,
                         args.block_device, args.signing_key, args.image)
