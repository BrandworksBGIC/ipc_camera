#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import sys
import hashlib
import struct
# from Cryptodome.Cipher import AES
from Crypto.Cipher import AES

# passphrase processing:
# From a key file: It will be truncated to the key size of the used cipher or
# the size given by -s and directly used as a binary key.
def generater_key(keyfile):
    with open(keyfile, "rb") as fd:
        return fd.read(-1)

def generater_iv(sector_num, salt):
# essiv:sha256
# IV(sector) = Esalt(sector) where salt = H(K)
#    cipher = AES.new(salt, AES.MODE_ECB)
#    return cipher.encrypt(struct.pack('<I', sector_num).ljust(AES.block_size, '\x00'))
#
# plain64: the initial vector is the 64-bit little-endian version of the sector
# number,padded with zeros if necessary
    return struct.pack('<Q', sector_num).ljust(AES.block_size, b'\x00')

def encrypt(ptext, key, mode, iv):
    cipher = AES.new(key, mode, iv)
    return cipher.encrypt(ptext)

# dm-crypt plain mode default cipher:
# aes-cbc-essiv:sha256, key: 256 bits, password hashing: ripemd160
def encrypt_fs(ifsname, ofsname, keyfile, sector_size):
    key = generater_key(keyfile)
    salt = hashlib.sha256(key).digest()
    sector_num = 0
    with open(ifsname, "rb") as fdi, open(ofsname, "wb") as fdo:
        while True:
            ptext = fdi.read(sector_size)
            if ptext == b'':
                break
            iv = generater_iv(sector_num, salt)
            sector_num = sector_num + 1
            ctext = encrypt(ptext, key, AES.MODE_CBC, iv)
            fdo.write(ctext)
        print("create " + ofsname)
    return

if __name__ == "__main__":
    ifsname = sys.argv[1]
    ofsname = sys.argv[2]
    keyfile = sys.argv[3]
    sector_size = int(sys.argv[4])
    encrypt_fs(ifsname, ofsname, keyfile, sector_size)
