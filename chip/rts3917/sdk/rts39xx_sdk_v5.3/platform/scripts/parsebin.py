#!/usr/bin/env python3
import os
import struct
import binascii
import sys
import zlib
import argparse


fhsize = 256              #size of file header
shsize = 32               #size of section header
feofsize = 4              #size of FEOF
cssize = 4                #size of checksum


def aparse():
    parse = argparse.ArgumentParser(description = 'A tool to parse linux.bin')
    parse.add_argument('-c','--checksum',
        help = 'If you want to checksum, add "-c".',
        action = 'store_true')
    parse.add_argument('path',
        help = 'Need a path.(Both absolute and relative paths work)')
    args = parse.parse_args()
    check = args.checksum
    path = args.path
    return check,path


def open_file(file_path):
    try:
        file = open(file_path,"rb")
    except IOError:
        print('The linux.bin file could not be found,please make sure the path is complete.')
        print('Both absolute and relative paths work.')
        print('eg: ./parsebin.py ~/sdk4.1/out/rts3916n_base/images/linux.bin')
        sys.exit()
    return file


def read_file_header(file):
    fhread = file.read(fhsize)
    return fhread


def read_section_header(file,shsite):
    file.seek(shsite,0)
    shread = file.read(shsize)
    return shread


def find_feof(file,feofsite):
    feof = b'FEOF'
    file.seek(feofsite,0)
    feofass = file.read(feofsize)
    if feofass == feof:
        print('\n')
        print('FEOF:           ',' '.join('%02X' % id for id in feofass[0:4]))
        return True
    else:
        return False

parse_boot_mode = {
        0 : 'normal',
        1 : 'crypto',
        2 : 'secure',
        3 : 'secure + crypto',
        }

def print_file_header(fh):
    print('\n')
    print('File Header')
    print('Magic Number:   ',' '.join('%02X' % id for id in fh[0:4]))
    print('IC Version:     ',fh[4] << 8 | fh[5])
    print('Bitmap:         ',' '.join('%02X' % id for id in fh[6:8]))
    print('Customer VID:   ',' '.join('%02X' % id for id in fh[8:10]))
    print('Customer PID:   ',' '.join('%02X' % id for id in fh[10:12]))
    print('Kernel Version: ','.'.join('%s' % id for id in fh[12:15]))
    print('SDK Version:    ','.'.join('%s' % id for id in fh[16:19]))
    print('SDK RC:         ',' '.join('%02X' % id for id in fh[19:20]))
    print('MCU FW Trunk Version: ',' '.join('%02X' % id for id in fh[20:24]))
    print('MCU FW Customer Version: ',' '.join('%02X' % id for id in fh[24:28]))
    uyear = fh[28] << 8 | fh[29]
    print('Uboot Version:   ',uyear,'.','.'.join('%s' % id for id in fh[30:32]),sep ='')
    print('Build Footprint: ',''.join('%c' % id for id in fh[32:64]),sep ='')
    byear = fh[64] << 8 | fh[65]
    print('Build Data:      ',byear,'/','/'.join('%s' % id for id in fh[66:68]),sep ='')
    print('Build Time:     ',':'.join('%s' % id for id in fh[68:72]))
    print('DDR Type:       ',' '.join('%02X' % id for id in fh[72:76]))
    print('Kernel Offset:  ',' '.join('%02X' % id for id in fh[76:84]))
    print('Rootfs Offset:  ',' '.join('%02X' % id for id in fh[84:92]))
    print('Boot Mode:      ','%d (%s)' %(fh[92],parse_boot_mode[fh[92]]))


sh_magic_number = {
    '0x626f6f74' : '(u-boot)',
    '0x6669726d' : '(mcu_fw)',
    '0x68636f6e' : '(hconf)',
    '0x6a667332' : '(jffs2)',
    '0x6c696e78' : '(kernel)',
    '0x726f6f74' : '(rootfs)',
    '0x6c646300' : '(ldc)',
    '0x64746200' : '(dtb)',
    '0x67707468' : '(gpt_head)',
    '0x67707474' : '(gpt_tail)',
    '0x636f6e66' : '(config)',
    '0x6c6f67ff' : '(log)',
    '0x726f6474' : '(rodata)'
    }


def print_section_header(sh):
    secsize = (sh[28] << 24 | sh[29] << 16 | sh[30] << 8 | sh[31])
    print('\n')
    print('Section Header')
    print('Magic Number:   ',' '.join('%02X' % id for id in sh[0:4]),end = ' ')
    mgnum = int.from_bytes(sh[0:4],byteorder='big')
    hexmgnum = hex(mgnum)
    print(sh_magic_number[hexmgnum])
    dyear = sh[4] << 8 | sh[5]
    print('Data & Time:     ',dyear,'/','/'.join('%s' % id for id in sh[6:8]),end = ' ',sep ='')
    print(':'.join('%s' % id for id in sh[8:11]))
    print('Partition Size: ',' '.join('%02X' % id for id in sh[12:16]))
    print('Offset:         ',' '.join('%02X' % id for id in sh[20:24]))
    print('Size:           ',secsize)
    slength = shsize + secsize + cssize
    return slength,secsize


def section_crc_check(crchead,crcsize):
    file.seek(crchead,0)
    align = 0x100000000
    crcsum = 0
    for i in range(crcsize):
        crcread = file.read(1)
        crcsum += struct.unpack("B",crcread)[0]
    checkcnt = (align - crcsum % align) & 0xFFFFFFFF
    file.seek(crchead + crcsize,0)
    csread = file.read(4)
    checksum = int.from_bytes(csread,byteorder='big')
    if checksum == checkcnt:
        print('Checksum:        Pass')
        return True
    else:
        print('Checksum:        Fail')
        return False


if __name__ == '__main__':
    (check,path) = aparse()
    file = open_file(path)
    fsize = os.path.getsize(path)
    fhread = read_file_header(file)
    print_file_header(fhread)
    readsite = fhsize
    while readsite < fsize:
        if find_feof(file,readsite) == 0:
            shread = read_section_header(file,readsite)
            (slength,secsize) = print_section_header(shread)
            if check == True:
                section_crc_check(readsite + shsize,secsize)
            readsite = slength + readsite
        else:
            break
    if check == False:
        print('\n')
        print('If you want to checksum, add "-c".')
        print('\n')
    file.close()
